# executor -- the single core-1 task (device pollers today; watch_gpio and the intent queue later).
# Bus-agnostic: a poller's sample() callback does its own bus locking, so this pulls in no bus driver.
idf_component_register(
    SRCS "executor.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
)
