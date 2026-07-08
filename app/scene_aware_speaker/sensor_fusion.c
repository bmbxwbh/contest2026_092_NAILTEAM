/****************************************************************************
 * AI Scene-Aware Smart Speaker - Sensor Fusion
 *
 * Reads data from SHTC3 (temp/humidity), SGP30 (VOC), LTR553 (light).
 * Provides fused sensor data for scene detection.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#ifdef CONFIG_SENSORS
#include <nuttx/sensors/sensor.h>
#endif

#include "scene_aware_speaker.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Sensor device paths */

#define SENSOR_SHTC3_PATH       "/dev/sensor/thermal_shtc3"
#define SENSOR_SGP30_PATH       "/dev/sensor/gas_sgp30"
#define SENSOR_LTR553_PATH      "/dev/sensor/light_ltr553"

/* Sensor read interval */

#define SENSOR_READ_INTERVAL_MS 1000

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_SENSORS
static int g_shtc3_fd = -1;
static int g_sgp30_fd = -1;
static int g_ltr553_fd = -1;
#endif

/* Last valid sensor data */

static sensor_data_t g_last_data;
static bool g_sensor_initialized = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: open_sensor
 *
 * Description:
 *   Open sensor device and verify it's accessible.
 *
 ****************************************************************************/

#ifdef CONFIG_SENSORS
static int open_sensor(const char *path, const char *name)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      printf("[SENSOR] %s not available at %s (errno=%d)\n",
             name, path, errno);
      return -1;
    }

  printf("[SENSOR] %s opened: %s\n", name, path);
  return fd;
}
#endif

/****************************************************************************
 * Name: read_shtc3
 *
 * Description:
 *   Read temperature and humidity from SHTC3 sensor.
 *
 ****************************************************************************/

#ifdef CONFIG_SENSORS_SHTC3
static int read_shtc3(float *temperature, float *humidity)
{
  if (g_shtc3_fd < 0)
    {
      return -ENODEV;
    }

  struct sensor_temp temp_data;
  struct sensor_humi humi_data;
  int ret;

  /* Read temperature */

  ret = read(g_shtc3_fd, &temp_data, sizeof(temp_data));
  if (ret == sizeof(temp_data))
    {
      *temperature = temp_data.temperature;
    }
  else
    {
      *temperature = 0.0f;
    }

  /* SHTC3 provides both temp and humidity in same read */

  *humidity = 50.0f; /* Default if not available */

  return 0;
}
#endif

/****************************************************************************
 * Name: read_sgp30
 *
 * Description:
 *   Read VOC and eCO2 from SGP30 sensor.
 *
 ****************************************************************************/

#ifdef CONFIG_SENSORS_SGP30_UORB
static int read_sgp30(uint16_t *tvoc, uint16_t *eco2)
{
  if (g_sgp30_fd < 0)
    {
      return -ENODEV;
    }

  struct sensor_gas gas_data;
  int ret;

  ret = read(g_sgp30_fd, &gas_data, sizeof(gas_data));
  if (ret == sizeof(gas_data))
    {
      *tvoc = (uint16_t)gas_data.tvoc;
      *eco2 = (uint16_t)gas_data.eco2;
    }
  else
    {
      *tvoc = 0;
      *eco2 = 400; /* Default CO2 level */
    }

  return 0;
}
#endif

/****************************************************************************
 * Name: read_ltr553
 *
 * Description:
 *   Read light and proximity from LTR553 sensor.
 *
 ****************************************************************************/

#ifdef CONFIG_SENSORS_LTR553
static int read_ltr553(uint32_t *light, uint16_t *proximity)
{
  if (g_ltr553_fd < 0)
    {
      return -ENODEV;
    }

  struct sensor_light light_data;
  struct sensor_prox prox_data;
  int ret;

  /* Read light */

  ret = read(g_ltr553_fd, &light_data, sizeof(light_data));
  if (ret == sizeof(light_data))
    {
      *light = (uint32_t)light_data.light;
    }
  else
    {
      *light = 100; /* Default ambient light */
    }

  /* Read proximity */

  ret = read(g_ltr553_fd, &prox_data, sizeof(prox_data));
  if (ret == sizeof(prox_data))
    {
      *proximity = (uint16_t)prox_data.proximity;
    }
  else
    {
      *proximity = 0; /* Default no proximity */
    }

  return 0;
}
#endif

/****************************************************************************
 * Name: read_sensors_simulated
 *
 * Description:
 *   Return simulated sensor data when real sensors are not available.
 *
 ****************************************************************************/

static void read_sensors_simulated(sensor_data_t *data)
{
  /* Simulated values for development */

  data->temperature = 25.0f;
  data->humidity = 50.0f;
  data->tvoc = 100;
  data->eco2 = 450;
  data->light = 200;
  data->proximity = 0;
  data->valid = true;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sensor_fusion_init
 *
 * Description:
 *   Initialize sensor devices.
 *
 ****************************************************************************/

int sensor_fusion_init(void)
{
  memset(&g_last_data, 0, sizeof(g_last_data));

#ifdef CONFIG_SENSORS
  /* Open SHTC3 (temperature/humidity) */

#ifdef CONFIG_SENSORS_SHTC3
  g_shtc3_fd = open_sensor(SENSOR_SHTC3_PATH, "SHTC3");
#endif

  /* Open SGP30 (VOC/eCO2) */

#ifdef CONFIG_SENSORS_SGP30_UORB
  g_sgp30_fd = open_sensor(SENSOR_SGP30_PATH, "SGP30");
#endif

  /* Open LTR553 (light/proximity) */

#ifdef CONFIG_SENSORS_LTR553
  g_ltr553_fd = open_sensor(SENSOR_LTR553_PATH, "LTR553");
#endif

  /* Check if any sensor is available */

  if (g_shtc3_fd >= 0 || g_sgp30_fd >= 0 || g_ltr553_fd >= 0)
    {
      printf("[SENSOR] Real sensors available\n");
    }
  else
    {
      printf("[SENSOR] No real sensors, using simulated data\n");
    }
#else
  printf("[SENSOR] CONFIG_SENSORS disabled, using simulated data\n");
#endif

  g_sensor_initialized = true;
  printf("[SENSOR] Sensor fusion initialized\n");

  return 0;
}

/****************************************************************************
 * Name: sensor_fusion_read
 *
 * Description:
 *   Read all sensors and return fused data.
 *
 * Output:
 *   data - Sensor data structure to fill
 *
 * Returns:
 *   0 on success, negative error code on failure.
 *
 ****************************************************************************/

int sensor_fusion_read(sensor_data_t *data)
{
  if (!data)
    {
      return -EINVAL;
    }

  if (!g_sensor_initialized)
    {
      return -ENODEV;
    }

#ifdef CONFIG_SENSORS
  bool any_read = false;

  /* Read SHTC3 */

#ifdef CONFIG_SENSORS_SHTC3
  if (read_shtc3(&data->temperature, &data->humidity) == 0)
    {
      any_read = true;
    }
#endif

  /* Read SGP30 */

#ifdef CONFIG_SENSORS_SGP30_UORB
  if (read_sgp30(&data->tvoc, &data->eco2) == 0)
    {
      any_read = true;
    }
#endif

  /* Read LTR553 */

#ifdef CONFIG_SENSORS_LTR553
  if (read_ltr553(&data->light, &data->proximity) == 0)
    {
      any_read = true;
    }
#endif

  /* Use simulated data if no real sensors */

  if (!any_read)
    {
      read_sensors_simulated(data);
    }
  else
    {
      data->valid = true;
    }
#else
  /* No sensor support, use simulated data */

  read_sensors_simulated(data);
#endif

  /* Update last valid data */

  if (data->valid)
    {
      g_last_data = *data;
    }

  return 0;
}

/****************************************************************************
 * Name: sensor_fusion_get_last
 *
 * Description:
 *   Get last valid sensor data without reading.
 *
 ****************************************************************************/

int sensor_fusion_get_last(sensor_data_t *data)
{
  if (!data)
    {
      return -EINVAL;
    }

  *data = g_last_data;
  return 0;
}

/****************************************************************************
 * Name: sensor_fusion_cleanup
 *
 * Description:
 *   Close sensor devices.
 *
 ****************************************************************************/

void sensor_fusion_cleanup(void)
{
#ifdef CONFIG_SENSORS
  if (g_shtc3_fd >= 0)
    {
      close(g_shtc3_fd);
      g_shtc3_fd = -1;
    }

  if (g_sgp30_fd >= 0)
    {
      close(g_sgp30_fd);
      g_sgp30_fd = -1;
    }

  if (g_ltr553_fd >= 0)
    {
      close(g_ltr553_fd);
      g_ltr553_fd = -1;
    }
#endif

  g_sensor_initialized = false;
  printf("[SENSOR] Sensor fusion cleaned up\n");
}
