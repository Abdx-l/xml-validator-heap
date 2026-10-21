#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char **items; size_t size, capacity; } Stack;
typedef struct { long *items; size_t size, capacity; } Heap;

static void *resize(void *p, size_t count, size_t width) {
    if (count > SIZE_MAX / width) { fputs("size overflow\n", stderr); exit(2); }
    void *next = realloc(p, count * width);
    if (!next) { fputs("out of memory\n", stderr); exit(2); }
    return next;
}

static char *copy_name(const char *start, size_t length) {
    char *s = malloc(length + 1);
    if (!s) { fputs("out of memory\n", stderr); exit(2); }
    memcpy(s, start, length);
    s[length] = 0;
    return s;
}

static void push(Stack *stack, char *name) {
    if (stack->size == stack->capacity) {
        stack->capacity = stack->capacity ? stack->capacity * 2 : 16;
        stack->items = resize(stack->items, stack->capacity, sizeof(char *));
    }
    stack->items[stack->size++] = name;
}

static int name_start(unsigned char c) {
    return isalpha(c) || c == '_' || c == ':';
}
static int name_char(unsigned char c) {
    return name_start(c) || isdigit(c) || c == '-' || c == '.';
}

static int fail(size_t pos, const char *message) {
    fprintf(stderr, "XML error at byte %zu: %s\n", pos, message);
    return 1;
}

static int validate(const char *s, size_t n) {
    Stack stack = {0};
    int root_count = 0, error = 0;
    for (size_t i = 0; i < n && !error;) {
        if (s[i] != '<') {
            if (!stack.size && !isspace((unsigned char)s[i]))
                error = fail(i, "text outside root");
            if (s[i] == '&') {
                size_t j = i + 1;
                while (j < n && j - i < 32 && s[j] != ';' &&
                       !isspace((unsigned char)s[j]) && s[j] != '<') j++;
                if (j == n || s[j] != ';') error = fail(i, "unclosed entity");
                else i = j;
            }
            i++;
            continue;
        }
        size_t at = i;
        if (i + 4 <= n && !memcmp(s + i, "<!--", 4)) {
            i += 4;
            while (i + 3 <= n && memcmp(s + i, "-->", 3)) i++;
            if (i + 3 > n) error = fail(at, "unclosed comment");
            else i += 3;
            continue;
        }
        if (i + 9 <= n && !memcmp(s + i, "<![CDATA[", 9)) {
            if (!stack.size) { error = fail(at, "CDATA outside root"); continue; }
            i += 9;
            while (i + 3 <= n && memcmp(s + i, "]]>", 3)) i++;
            if (i + 3 > n) error = fail(at, "unclosed CDATA");
            else i += 3;
            continue;
        }
        if (i + 2 <= n && !memcmp(s + i, "<?", 2)) {
            i += 2;
            while (i + 2 <= n && memcmp(s + i, "?>", 2)) i++;
            if (i + 2 > n) error = fail(at, "unclosed processing instruction");
            else i += 2;
            continue;
        }
        if (i + 2 <= n && s[i + 1] == '!') {
            error = fail(at, "DTD/declaration unsupported");
            continue;
        }
        i++;
        int closing = i < n && s[i] == '/';
        if (closing) i++;
        size_t begin = i;
        if (i >= n || !name_start((unsigned char)s[i])) {
            error = fail(at, "expected tag name");
            continue;
        }
        while (i < n && name_char((unsigned char)s[i])) i++;
        char *name = copy_name(s + begin, i - begin);
        int quote = 0, self_closing = 0;
        size_t last_nonspace = i;
        for (; i < n; i++) {
            if (quote) {
                if (s[i] == quote) quote = 0;
            } else if (s[i] == '"' || s[i] == '\'') {
                quote = s[i];
            } else if (s[i] == '>') {
                self_closing = s[last_nonspace] == '/';
                break;
            } else if (!isspace((unsigned char)s[i])) {
                last_nonspace = i;
            }
        }
        if (i == n) error = fail(at, "unclosed tag");
        else if (closing) {
            if (self_closing || (s[last_nonspace] != '/' &&
                                 last_nonspace != begin + strlen(name)))
                error = fail(at, "invalid closing tag");
            else if (!stack.size || strcmp(stack.items[stack.size - 1], name))
                error = fail(at, "mismatched closing tag");
            else free(stack.items[--stack.size]);
        } else {
            if (!stack.size && ++root_count > 1)
                error = fail(at, "multiple root elements");
            if (!self_closing && !error) push(&stack, name), name = NULL;
        }
        free(name);
        i++;
    }
    if (!error && stack.size) error = fail(n, "unclosed element");
    if (!error && root_count != 1) error = fail(n, "missing root element");
    for (size_t i = 0; i < stack.size; i++) free(stack.items[i]);
    free(stack.items);
    if (!error) puts("valid");
    return error;
}

static int load_file(const char *path, char **buffer, size_t *length) {
    FILE *file = fopen(path, "rb");
    if (!file) { perror(path); return 1; }
    size_t size = 0, capacity = 4096;
    char *data = malloc(capacity + 1);
    if (!data) { fclose(file); return 2; }
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (size == capacity) {
            if (capacity > SIZE_MAX / 2 - 1) { free(data); fclose(file); return 2; }
            capacity *= 2;
            data = resize(data, capacity + 1, 1);
        }
        data[size++] = (char)ch;
    }
    if (ferror(file)) { perror(path); free(data); fclose(file); return 1; }
    fclose(file);
    data[size] = 0;
    *buffer = data;
    *length = size;
    return 0;
}

static void heap_add(Heap *heap, long number) {
    if (heap->size == heap->capacity) {
        heap->capacity = heap->capacity ? heap->capacity * 2 : 16;
        heap->items = resize(heap->items, heap->capacity, sizeof(long));
    }
    size_t i = heap->size++;
    while (i && heap->items[(i - 1) / 2] < number) {
        heap->items[i] = heap->items[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    heap->items[i] = number;
}

static void emit_node(const Heap *heap, size_t i, int depth) {
    for (int j = 0; j < depth; j++) fputs("  ", stdout);
    printf("<node id=\"%zu\" value=\"%ld\"", i, heap->items[i]);
    if (i > (SIZE_MAX - 2) / 2 || 2 * i + 1 >= heap->size) {
        puts("/>");
        return;
    }
    puts(">");
    emit_node(heap, 2 * i + 1, depth + 1);
    if (2 * i + 2 < heap->size) emit_node(heap, 2 * i + 2, depth + 1);
    for (int j = 0; j < depth; j++) fputs("  ", stdout);
    puts("</node>");
}

static int export_heap(const char *path) {
    FILE *file = fopen(path, "r");
    Heap heap = {0};
    if (!file) { perror(path); return 1; }
    char token[128];
    while (fscanf(file, "%127s", token) == 1) {
        char *end;
        errno = 0;
        long value = strtol(token, &end, 10);
        if (errno || end == token || *end) {
            fprintf(stderr, "invalid integer: %s\n", token);
            free(heap.items);
            fclose(file);
            return 1;
        }
        heap_add(&heap, value);
    }
    fclose(file);
    puts("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    puts("<heap>");
    if (heap.size) emit_node(&heap, 0, 1);
    puts("</heap>");
    free(heap.items);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s validate|heap FILE\n", argv[0]);
        return 1;
    }
    if (!strcmp(argv[1], "heap")) return export_heap(argv[2]);
    if (!strcmp(argv[1], "validate")) {
        char *data;
        size_t length;
        int result = load_file(argv[2], &data, &length);
        if (result) return result;
        result = validate(data, length);
        free(data);
        return result;
    }
    fputs("expected validate or heap\n", stderr);
    return 1;
}

