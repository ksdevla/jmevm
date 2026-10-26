/*
 * jmevm command-line runner.
 *
 *   jmevm --version
 *   jmevm --dump-class FILE...
 *   jmevm [--heap MB] [--epoch MS] [--gc-stress] [--print-ticks] [--boot PATH] -cp PATH MAIN [ARGS...]
 *
 * PATH lists class directories separated by ';' on Windows and ':' elsewhere.
 * Classes are looked up in --boot (the class library) first, then in -cp.
 * The exit code is the program's: System.exit(), 1 after an uncaught
 * exception, 0 otherwise; 2 for usage errors, 3 when the VM cannot start or
 * the program deadlocks.
 *
 * Self-checks (testing aids; see check.h), which exit with 4 on a failure:
 *   --snapshot-check STEP  snapshot every STEP ticks; each must restore exactly
 *   --rewind-check STEP    the same through a rewind ring
 *   --history-check STEP   the same through the every-frame history
 *   --check-stats          print snapshot and ring sizes on stderr
 *   --test-host            add the test host's natives and state (testhost.h)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "classfile.h"
#include "testhost.h"

#ifdef _WIN32
#define PATH_SEP ';'
#else
#define PATH_SEP ':'
#endif

enum { EXIT_USAGE = 2, EXIT_VM = 3 };

static void usage(void) {
	fputs("usage: jmevm --version\n"
	      "       jmevm --dump-class FILE...\n"
	      "       jmevm [--heap MB] [--epoch MS] [--gc-stress] [--print-ticks] [--boot PATH] -cp PATH MAIN [ARGS...]\n",
	      stderr);
}

/* Reads a whole file; NULL if it cannot be read. */
static u8 *read_file(const char *path, size_t *len) {
	FILE *f = fopen(path, "rb");
	if (f == NULL) return NULL;
	u8 *buf = NULL;
	size_t n = 0, cap = 0;
	for (;;) {
		if (n == cap) {
			cap = cap ? cap * 2 : 64 * 1024;
			u8 *b = realloc(buf, cap);
			if (b == NULL) {
				free(buf);
				fclose(f);
				return NULL;
			}
			buf = b;
		}
		size_t got = fread(buf + n, 1, cap - n, f);
		n += got;
		if (got == 0) break;
	}
	int err = ferror(f);
	fclose(f);
	if (err) {
		free(buf);
		return NULL;
	}
	*len = n;
	return buf;
}

/* ---- --dump-class ---- */

static void print_utf8(Utf8 s) {
	fwrite(s.bytes, 1, s.len, stdout);
}

static void print_flags(u16 access, int is_method) {
	static const struct { u16 bit; const char *name; } flags[] = {
		{ACC_PUBLIC, "public"}, {ACC_PRIVATE, "private"}, {ACC_PROTECTED, "protected"},
		{ACC_STATIC, "static"}, {ACC_FINAL, "final"}, {ACC_NATIVE, "native"},
		{ACC_ABSTRACT, "abstract"},
	};
	for (size_t i = 0; i < sizeof flags / sizeof flags[0]; i++)
		if (access & flags[i].bit) printf("%s ", flags[i].name);
	if (is_method && (access & ACC_SYNCHRONIZED)) printf("synchronized ");
}

static int dump_class(const char *path) {
	size_t len;
	u8 *data = read_file(path, &len);
	if (data == NULL) {
		fprintf(stderr, "%s: cannot read\n", path);
		return 1;
	}
	ClassFile *cf;
	const char *why;
	JmeStatus st = classfile_parse(data, len, &cf, &why);
	free(data);
	if (st != JME_OK) {
		fprintf(stderr, "%s: %s: %s\n", path, jme_status_name(st), why ? why : "");
		return 1;
	}

	printf("%s ", cf->access & ACC_INTERFACE ? "interface" : "class");
	print_utf8(cf->name);
	printf(" (version %u.%u, %u constants)", cf->major, cf->minor, cf->cp_count);
	if (cf->super_name.bytes != NULL) {
		printf(" extends ");
		print_utf8(cf->super_name);
	}
	for (u32 i = 0; i < cf->interface_count; i++) {
		printf(i == 0 ? " implements " : ", ");
		print_utf8(cf->interfaces[i]);
	}
	if (cf->source_file.bytes != NULL) {
		printf(" from ");
		print_utf8(cf->source_file);
	}
	printf("\n");
	for (u32 i = 0; i < cf->field_count; i++) {
		const FieldInfo *f = &cf->fields[i];
		printf("  field ");
		print_flags(f->access, 0);
		print_utf8(f->name);
		printf(" ");
		print_utf8(f->desc);
		if (f->const_index) printf(" = #%u", f->const_index);
		printf("\n");
	}
	for (u32 i = 0; i < cf->method_count; i++) {
		const MethodInfo *m = &cf->methods[i];
		printf("  method ");
		print_flags(m->access, 1);
		print_utf8(m->name);
		print_utf8(m->desc);
		if (m->code != NULL)
			printf(" code=%u stack=%u locals=%u handlers=%u", m->code->code_len,
					m->code->max_stack, m->code->max_locals, m->code->handler_count);
		printf("\n");
	}
	classfile_free(cf);
	return 0;
}

/* ---- class source over directories ---- */

typedef struct Loaded {
	struct Loaded *next;
	char *name;
	u8 *data;
	size_t len;
} Loaded;

/* Class files are read once and kept until exit: VMs point into them, and
 * every VM that loads the class (snapshot loads replay class loading) gets
 * the same bytes. */
typedef struct DirSource {
	const char *boot;
	const char *cp;
	Loaded *loaded;
} DirSource;

/* Tries every directory of path for name. */
static u8 *find_in_path(const char *path, const char *name, size_t *len) {
	if (path == NULL) return NULL;
	const char *p = path;
	while (*p) {
		const char *end = strchr(p, PATH_SEP);
		size_t dlen = end ? (size_t)(end - p) : strlen(p);
		if (dlen > 0) {
			size_t n = dlen + 1 + strlen(name) + sizeof(".class");
			char *file = malloc(n);
			if (file == NULL) return NULL;
			snprintf(file, n, "%.*s/%s.class", (int)dlen, p, name);
			u8 *data = read_file(file, len);
			free(file);
			if (data != NULL) return data;
		}
		if (end == NULL) break;
		p = end + 1;
	}
	return NULL;
}

static JmeStatus dir_source(void *user, const char *name, const uint8_t **data, size_t *len) {
	DirSource *s = user;
	for (Loaded *l = s->loaded; l != NULL; l = l->next) {
		if (strcmp(l->name, name) == 0) {
			*data = l->data;
			*len = l->len;
			return JME_OK;
		}
	}
	u8 *bytes = find_in_path(s->boot, name, len);
	if (bytes == NULL) bytes = find_in_path(s->cp, name, len);
	if (bytes == NULL) return JME_E_NOTFOUND;
	Loaded *l = malloc(sizeof(Loaded));
	char *copy = malloc(strlen(name) + 1);
	if (l == NULL || copy == NULL) {
		free(l);
		free(copy);
		free(bytes);
		return JME_E_NOMEM;
	}
	strcpy(copy, name);
	u8 *fit = realloc(bytes, *len ? *len : 1);
	l->name = copy;
	l->data = fit != NULL ? fit : bytes;
	l->len = *len;
	l->next = s->loaded;
	s->loaded = l;
	*data = l->data;
	return JME_OK;
}

static void console(void *user, int stream, const char *data, size_t len) {
	(void)user;
	FILE *f = stream == 2 ? stderr : stdout;
	fwrite(data, 1, len, f);
	/* keep stdout and stderr in program order */
	fflush(f);
}

static void free_loaded(DirSource *source) {
	while (source->loaded != NULL) {
		Loaded *next = source->loaded->next;
		free(source->loaded->name);
		free(source->loaded->data);
		free(source->loaded);
		source->loaded = next;
	}
}

static int run(int argc, char **argv) {
	JmeOptions options;
	jme_options_default(&options);
	DirSource source = {NULL, NULL, NULL};
	int print_ticks = 0, check_stats = 0, check_rewind = 0, test_host = 0;
	unsigned long long check_step = 0;
	int i = 1;
	for (; i < argc && argv[i][0] == '-'; i++) {
		const char *a = argv[i];
		if (strcmp(a, "--gc-stress") == 0) {
			options.gc_stress = 1;
			continue;
		}
		if (strcmp(a, "--print-ticks") == 0) {
			print_ticks = 1;
			continue;
		}
		if (strcmp(a, "--check-stats") == 0) {
			check_stats = 1;
			continue;
		}
		if (strcmp(a, "--test-host") == 0) {
			test_host = 1;
			continue;
		}
		if (i + 1 >= argc) {
			usage();
			return EXIT_USAGE;
		}
		if (strcmp(a, "--snapshot-check") == 0 || strcmp(a, "--rewind-check") == 0
				|| strcmp(a, "--history-check") == 0) {
			check_rewind = a[2] == 'r' ? 1 : a[2] == 'h' ? 2 : 0;
			check_step = strtoull(argv[++i], NULL, 10);
			if (check_step == 0) {
				usage();
				return EXIT_USAGE;
			}
		} else if (strcmp(a, "--heap") == 0) {
			options.heap_bytes = (u32)strtoul(argv[++i], NULL, 10) << 20;
		} else if (strcmp(a, "--epoch") == 0) {
			options.epoch_ms = strtoll(argv[++i], NULL, 10);
		} else if (strcmp(a, "--boot") == 0) {
			source.boot = argv[++i];
		} else if (strcmp(a, "-cp") == 0) {
			source.cp = argv[++i];
		} else {
			fprintf(stderr, "jmevm: unknown option %s\n", a);
			usage();
			return EXIT_USAGE;
		}
	}
	if (i >= argc || source.cp == NULL) {
		usage();
		return EXIT_USAGE;
	}
	const char *main_class = argv[i++];

	if (check_step != 0) {
		CheckSetup setup = {&options, dir_source, &source, main_class, argc - i,
				(const char *const *)(argv + i), check_step, check_rewind, check_stats,
				test_host ? testhost_prepare : NULL, test_host ? testhost_step : NULL, NULL};
		int code = check_snapshots(&setup);
		free_loaded(&source);
		testhost_free_all();
		return code;
	}

	JmeVM *vm;
	int exit_code = 0, deadlock = 0;
	JmeStatus st = jme_create(&options, &vm);
	if (st == JME_OK) {
		jme_set_class_source(vm, dir_source, &source);
		jme_set_console(vm, console, NULL);
		if (test_host) testhost_prepare(vm, NULL);
		st = jme_start(vm, main_class, argc - i, (const char *const *)(argv + i));
		if (st == JME_OK && test_host) {
			/* The test host feeds input between steps of one emulated ms. */
			while (!jme_finished(vm, NULL) && st == JME_OK) {
				testhost_step(vm, NULL);
				st = jme_run_until(vm, jme_ticks(vm) + options.ticks_per_ms);
			}
		}
		/* Ticks never reach UINT64_MAX: this returns when the program ends,
		 * or when every thread waits for something that cannot happen. */
		if (st == JME_OK) st = jme_run_until(vm, UINT64_MAX);
		if (st == JME_OK) deadlock = !jme_finished(vm, &exit_code);
		if (st == JME_OK && print_ticks) printf("ticks %llu\n", (unsigned long long)jme_ticks(vm));
		jme_destroy(vm);
	}
	free_loaded(&source);
	testhost_free_all();
	if (st == JME_E_NOTFOUND) {
		fprintf(stderr, "jmevm: cannot find %s, its main method or the class library\n", main_class);
		return EXIT_VM;
	}
	if (st != JME_OK) {
		fprintf(stderr, "jmevm: %s\n", jme_status_name(st));
		return EXIT_VM;
	}
	if (deadlock) {
		fprintf(stderr, "jmevm: deadlock: every thread is waiting and none can wake it\n");
		return EXIT_VM;
	}
	return exit_code;
}

int main(int argc, char **argv) {
	if (argc == 2 && strcmp(argv[1], "--version") == 0) {
		printf("jmevm %s\n", jme_version());
		return 0;
	}
	if (argc >= 2 && strcmp(argv[1], "--dump-class") == 0) {
		if (argc == 2) {
			usage();
			return EXIT_USAGE;
		}
		int failed = 0;
		for (int i = 2; i < argc; i++) failed |= dump_class(argv[i]);
		return failed;
	}
	return run(argc, argv);
}
