#include <stddef.h>

#include "jmevm/jmevm.h"
#include "unit.h"

void test_api_create_destroy(void) {
	JmeVM *vm = NULL;
	CHECK_EQ(jme_create(NULL, &vm), JME_OK);
	CHECK(vm != NULL);
	if (vm == NULL) return;
	CHECK_EQ(jme_ticks(vm), 0);
	/* No class source yet. */
	CHECK_EQ(jme_start(vm, "Main", 0, NULL), JME_E_STATE);
	jme_destroy(vm);
}

void test_api_rejects_bad_options(void) {
	JmeOptions o;
	JmeVM *vm = (JmeVM *)&o;
	jme_options_default(&o);
	o.heap_bytes = 1000;
	CHECK_EQ(jme_create(&o, &vm), JME_E_ARGUMENT);
	CHECK(vm == NULL);
	jme_options_default(&o);
	o.ticks_per_ms = 0;
	CHECK_EQ(jme_create(&o, &vm), JME_E_ARGUMENT);
}
