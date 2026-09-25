/* model_runner.c -- pick the execution model for this build's project type, in one place. The whole
 * point of Phase 0.5: the models are peers selected here, so event-driven joins as a third arm
 * rather than a third #ifdef woven through the boot path. §8.2. */
#include "model_runner.h"
#include "init_loop.h"     /* run_init_loop */
#include "web_server.h"    /* run_web_server */
#include "event_driven.h"  /* run_event_driven */

#if defined(PHP_PROJECT_WEB_SERVER)
static const model_runner_t s_runner = { .run = run_web_server };
#elif defined(PHP_PROJECT_EVENT_DRIVEN)
static const model_runner_t s_runner = { .run = run_event_driven };
#else
static const model_runner_t s_runner = { .run = run_init_loop };
#endif

const model_runner_t *model_runner_current(void)
{
    return &s_runner;
}
