
idf_component_register(
	INCLUDE_DIRS .
	SRCS
		main.c
		settings.c
		wifi/wifi.c
		wifi/webserver.c
		wifi/api.c
		wifi/mqtt.c
		utils/bus.c
		utils/utils.c
		utils/spiffs.c
		utils/nvs.c
		utils/json.c
		utils/send_data.c
		utils/common.c
		zigbee/zigbee_init.c
		modules/sensor_init.c
		modules/sensors/gpioOUT/sensor_gpioOUT.c
		modules/sensors/gpioIN/sensor_gpioin.c
		modules/sensors/_example/sensor_example.c
		modules/sensors/aht/sensor_aht.c
		modules/sensors/dht/sensor_dht.c
		modules/sensors/bmp280/sensor_bmp280.c
		modules/sensors/ds18b20/sensor_ds18b20.c

		modules/virtual/battery/battery.c
		modules/virtual/deepsleep/deepsleep.c
		
		modules/exec/led_light/led_light.c
		modules/exec/led_light/light.c
		modules/exec/led_light/crc16.c

		effects/effect.c
		effects/surface.c
		effects/input.c
		effects/palettes.c
		
		effects/crazybees.c
		effects/dna.c
		effects/fire.c
		effects/lamp.c
		effects/lava.c
		effects/matrix.c
		effects/plasma_waves.c
		effects/rain.c
		effects/rainbow.c
		effects/rays.c
		effects/sparkles.c
		effects/waterfall.c
		effects/metaballs.c
		effects/water.c
		effects/holiday.c
		effects/hoop.c
		effects/twister.c
		effects/flower.c
		effects/polar_lights.c
		
		effects/relax.c
		effects/wheel.c
		effects/peacock.c
		effects/diagonals.c
		effects/whirls.c
		effects/sun.c
		effects/spaceships.c
		effects/fireflies.c
		effects/tunnel.c
		effects/dunes.c
		effects/octopus.c
		effects/nuclear.c


		
)

spiffs_create_partition_image(storage ../spiffs_storage FLASH_IN_PROJECT)
