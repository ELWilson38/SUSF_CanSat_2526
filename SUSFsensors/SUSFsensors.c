/**
 * SUSFsensors.c
 * Raspberry Pi Pico - Multi-Sensor Data Logger
 *
 * Sensors:
 *  - DHT11  : Temperature & Humidity        -> GPIO 15 (single-wire)
 *  - DS1307 : Real-Time Clock (UK time)     -> I2C (SDA=GP4, SCL=GP5)
 *  - MPU-6050/ITG: Accel + Gyro            -> I2C (SDA=GP4, SCL=GP5)
 *  - LSM6DSO: Accel + Gyro (AltIMU-10 v6) -> I2C (SDA=GP4, SCL=GP5)
 *  - LIS3MDL: Compass/Magnetometer         -> I2C (SDA=GP4, SCL=GP5)
 *  - LPS22DF: Barometric Altimeter         -> I2C (SDA=GP4, SCL=GP5)
 *
 * Serial: 115200 baud via USB CDC
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

/* ─────────────────────────── Pin / Bus Config ─────────────────────────── */
#define I2C_PORT    i2c0
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define DHT_PIN     15

/* ─────────────────────────── I2C Addresses ────────────────────────────── */
#define DS1307_ADDR  0x68
#define MPU6050_ADDR 0x69   /* AD0 pulled HIGH → 0x69 avoids DS1307 conflict */
#define LSM6DSO_ADDR 0x6A
#define LIS3MDL_ADDR 0x1C
#define LPS22DF_ADDR 0x5C

/* ─────────────────────────── RTC Time Setting ──────────────────────────── */
/*
 * Set SET_RTC_TIME to 1 to write the time below to the DS1307 on boot.
 * After flashing once with SET_RTC_TIME 1, change it back to 0 and reflash
 * so the clock is not reset every time the Pico powers on.
 */
#define SET_RTC_TIME 1

#define RTC_SEC   25      /* seconds - set slightly ahead of your actual time */
#define RTC_MIN   22
#define RTC_HOUR  22
#define RTC_DAY   7       /* 1=Mon 2=Tue 3=Wed 4=Thu 5=Fri 6=Sat 7=Sun      */
#define RTC_DATE  1
#define RTC_MONTH 3
#define RTC_YEAR  26      /* last 2 digits e.g. 26 = 2026                    */

/* ═══════════════════════════════════════════════════════════════════════════
   I2C Helpers
   ═══════════════════════════════════════════════════════════════════════════ */

static bool i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_write_blocking(I2C_PORT, addr, buf, 2, false) == 2;
}

static bool i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    if (i2c_write_blocking(I2C_PORT, addr, &reg, 1, true) != 1) return false;
    return i2c_read_blocking(I2C_PORT, addr, buf, len, false) == (int)len;
}

/* ═══════════════════════════════════════════════════════════════════════════
   I2C Bus Scanner
   ═══════════════════════════════════════════════════════════════════════════ */

static void i2c_scan(void) {
    printf("  I2C Bus Scan (shows all connected devices):\n");
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy;
        int ret = i2c_read_blocking(I2C_PORT, addr, &dummy, 1, false);
        if (ret >= 0) {
            printf("    Found device at 0x%02X\n", addr);
            found++;
        }
    }
    if (found == 0)
        printf("    !! No I2C devices found - check SDA/SCL wiring!\n");
    else
        printf("  Total: %d device(s) found.\n", found);
}

/* ═══════════════════════════════════════════════════════════════════════════
   DHT11 - single-wire bit-bang
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    float temperature;
    float humidity;
    bool  valid;
} DHT11Data;

static void dht11_set_output(void) { gpio_set_dir(DHT_PIN, GPIO_OUT); }
static void dht11_set_input(void)  { gpio_set_dir(DHT_PIN, GPIO_IN); gpio_pull_up(DHT_PIN); }

DHT11Data dht11_read(void) {
    DHT11Data result = {0, 0, false};
    uint8_t data[5] = {0};
    uint32_t timeout;

    /* Host start: pull low 18 ms, then release */
    dht11_set_output();
    gpio_put(DHT_PIN, 0);
    sleep_ms(18);
    gpio_put(DHT_PIN, 1);
    sleep_us(30);
    dht11_set_input();

    /* Wait for sensor response */
    timeout = 0;
    while (gpio_get(DHT_PIN) == 1) { if (++timeout > 10000) return result; sleep_us(1); }
    while (gpio_get(DHT_PIN) == 0) { if (++timeout > 10000) return result; sleep_us(1); }
    while (gpio_get(DHT_PIN) == 1) { if (++timeout > 10000) return result; sleep_us(1); }

    /* Read 40 bits */
    for (int i = 0; i < 40; i++) {
        timeout = 0;
        while (gpio_get(DHT_PIN) == 0) { if (++timeout > 10000) return result; sleep_us(1); }
        sleep_us(40);
        if (gpio_get(DHT_PIN)) data[i / 8] |= (1 << (7 - (i % 8)));
        timeout = 0;
        while (gpio_get(DHT_PIN) == 1) { if (++timeout > 10000) return result; sleep_us(1); }
    }

    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) return result;

    result.humidity    = (float)data[0] + (float)data[1] * 0.1f;
    result.temperature = (float)data[2] + (float)data[3] * 0.1f;
    result.valid = true;
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
   DS1307 - Real-Time Clock
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t sec, min, hour, day, date, month;
    uint16_t year;
    bool valid;
} DS1307Data;

static uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }
static uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }

static void ds1307_set_time(void) {
    uint8_t buf[8];
    buf[0] = 0x00;
    buf[1] = dec2bcd(RTC_SEC);
    buf[2] = dec2bcd(RTC_MIN);
    buf[3] = dec2bcd(RTC_HOUR);
    buf[4] = dec2bcd(RTC_DAY);
    buf[5] = dec2bcd(RTC_DATE);
    buf[6] = dec2bcd(RTC_MONTH);
    buf[7] = dec2bcd(RTC_YEAR);
    i2c_write_blocking(I2C_PORT, DS1307_ADDR, buf, 8, false);
    printf("  DS1307 time set: %02d/%02d/20%02d %02d:%02d:%02d\n",
           RTC_DATE, RTC_MONTH, RTC_YEAR, RTC_HOUR, RTC_MIN, RTC_SEC);
}

DS1307Data ds1307_read(void) {
    DS1307Data r = {0};
    uint8_t buf[7];
    if (!i2c_read_regs(DS1307_ADDR, 0x00, buf, 7)) return r;
    r.sec   = bcd2dec(buf[0] & 0x7F);
    r.min   = bcd2dec(buf[1]);
    r.hour  = bcd2dec(buf[2] & 0x3F);
    r.day   = bcd2dec(buf[3]);
    r.date  = bcd2dec(buf[4]);
    r.month = bcd2dec(buf[5]);
    r.year  = 2000 + bcd2dec(buf[6]);
    r.valid = true;
    return r;
}

static const char *day_name(uint8_t d) {
    const char *days[] = {"?","Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    return (d < 8) ? days[d] : days[0];
}

/* ═══════════════════════════════════════════════════════════════════════════
   MPU-6050 - Accelerometer + Gyroscope
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    float ax, ay, az, gx, gy, gz, temp;
    bool  valid;
} MPU6050Data;

static bool mpu6050_init(void) {
    return i2c_write_reg(MPU6050_ADDR, 0x6B, 0x00);
}

MPU6050Data mpu6050_read(void) {
    MPU6050Data r = {0};
    uint8_t buf[14];
    if (!i2c_read_regs(MPU6050_ADDR, 0x3B, buf, 14)) return r;
    int16_t ax_raw = (int16_t)((buf[0]  << 8) | buf[1]);
    int16_t ay_raw = (int16_t)((buf[2]  << 8) | buf[3]);
    int16_t az_raw = (int16_t)((buf[4]  << 8) | buf[5]);
    int16_t t_raw  = (int16_t)((buf[6]  << 8) | buf[7]);
    int16_t gx_raw = (int16_t)((buf[8]  << 8) | buf[9]);
    int16_t gy_raw = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t gz_raw = (int16_t)((buf[12] << 8) | buf[13]);
    r.ax = ax_raw / 16384.0f; r.ay = ay_raw / 16384.0f; r.az = az_raw / 16384.0f;
    r.gx = gx_raw / 131.0f;  r.gy = gy_raw / 131.0f;  r.gz = gz_raw / 131.0f;
    r.temp = t_raw / 340.0f + 36.53f;
    r.valid = true;
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════
   LSM6DSO - Accel + Gyro (AltIMU-10 v6)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    float ax, ay, az, gx, gy, gz;
    bool  valid;
} LSM6DSOData;

static bool lsm6dso_init(void) {
    uint8_t who;
    if (!i2c_read_regs(LSM6DSO_ADDR, 0x0F, &who, 1)) {
        printf("  LSM6DSO: no response at 0x%02X\n", LSM6DSO_ADDR);
        return false;
    }
    printf("  LSM6DSO WHO_AM_I=0x%02X (expect 0x6C)\n", who);
    if (!i2c_write_reg(LSM6DSO_ADDR, 0x10, 0x42)) return false;
    if (!i2c_write_reg(LSM6DSO_ADDR, 0x11, 0x44)) return false;
    return true;
}

LSM6DSOData lsm6dso_read(void) {
    LSM6DSOData r = {0};
    uint8_t buf[12];
    if (!i2c_read_regs(LSM6DSO_ADDR, 0x22, buf, 12)) return r;
    int16_t gx_raw = (int16_t)((buf[1]  << 8) | buf[0]);
    int16_t gy_raw = (int16_t)((buf[3]  << 8) | buf[2]);
    int16_t gz_raw = (int16_t)((buf[5]  << 8) | buf[4]);
    int16_t ax_raw = (int16_t)((buf[7]  << 8) | buf[6]);
    int16_t ay_raw = (int16_t)((buf[9]  << 8) | buf[8]);
    int16_t az_raw = (int16_t)((buf[11] << 8) | buf[10]);
    r.gx = gx_raw * 0.01750f; r.gy = gy_raw * 0.01750f; r.gz = gz_raw * 0.01750f;
    r.ax = ax_raw * 0.000122f; r.ay = ay_raw * 0.000122f; r.az = az_raw * 0.000122f;
    r.valid = true;
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════
   LIS3MDL - Magnetometer / Compass
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    float mx, my, mz;
    bool  valid;
} LIS3MDLData;

static bool lis3mdl_init(void) {
    uint8_t who;
    if (!i2c_read_regs(LIS3MDL_ADDR, 0x0F, &who, 1)) {
        printf("  LIS3MDL: no response at 0x%02X\n", LIS3MDL_ADDR);
        return false;
    }
    printf("  LIS3MDL WHO_AM_I=0x%02X (expect 0x3D)\n", who);
    if (!i2c_write_reg(LIS3MDL_ADDR, 0x20, 0xF0)) return false;
    if (!i2c_write_reg(LIS3MDL_ADDR, 0x21, 0x00)) return false;
    if (!i2c_write_reg(LIS3MDL_ADDR, 0x22, 0x00)) return false;
    if (!i2c_write_reg(LIS3MDL_ADDR, 0x23, 0x0C)) return false;
    return true;
}

LIS3MDLData lis3mdl_read(void) {
    LIS3MDLData r = {0};
    uint8_t buf[6];
    if (!i2c_read_regs(LIS3MDL_ADDR, 0x28 | 0x80, buf, 6)) return r;
    int16_t mx_raw = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t my_raw = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t mz_raw = (int16_t)((buf[5] << 8) | buf[4]);
    r.mx = mx_raw / 6842.0f; r.my = my_raw / 6842.0f; r.mz = mz_raw / 6842.0f;
    r.valid = true;
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════
   LPS22DF - Pressure / Altitude
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    float pressure, altitude, temperature;
    bool  valid;
} LPS22DFData;

static bool lps22df_init(void) {
    uint8_t who;
    if (!i2c_read_regs(LPS22DF_ADDR, 0x0F, &who, 1)) {
        printf("  LPS22DF: no response at 0x%02X\n", LPS22DF_ADDR);
        return false;
    }
    printf("  LPS22DF WHO_AM_I=0x%02X (expect 0xB4)\n", who);
    if (!i2c_write_reg(LPS22DF_ADDR, 0x10, 0x28)) return false;
    return true;
}

LPS22DFData lps22df_read(void) {
    LPS22DFData r = {0};
    uint8_t buf[5];
    if (!i2c_read_regs(LPS22DF_ADDR, 0x28, buf, 5)) return r;
    int32_t p_raw = ((int32_t)buf[2] << 16) | ((int32_t)buf[1] << 8) | buf[0];
    int16_t t_raw = (int16_t)((buf[4] << 8) | buf[3]);
    r.pressure    = (float)p_raw / 4096.0f;
    r.temperature = (float)t_raw / 100.0f;
    r.altitude    = 44330.0f * (1.0f - __builtin_powf(r.pressure / 1013.25f, 0.1903f));
    r.valid = true;
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════
   Display Helpers
   ═══════════════════════════════════════════════════════════════════════════ */

static void print_divider(char c, int len) {
    for (int i = 0; i < len; i++) putchar(c);
    putchar('\n');
}
static void print_header(const char *t)  { print_divider('=',60); printf("  %s\n",t); print_divider('=',60); }
static void print_section(const char *t) { print_divider('-',60); printf("  [ %s ]\n",t); print_divider('-',60); }

/* ═══════════════════════════════════════════════════════════════════════════
   Main
   ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    stdio_init_all();

    /* Wait for USB serial to connect - countdown so you can open monitor */
    sleep_ms(1000);
    for (int i = 10; i > 0; i--) {
        printf("Starting in %d seconds... (open Serial Monitor now!)\n", i);
        sleep_ms(1000);
    }

    /* ── I2C init at 100 kHz (more reliable than 400 kHz) ── */
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    /* ── DHT11 GPIO init ── */
    gpio_init(DHT_PIN);
    gpio_put(DHT_PIN, 1);
    dht11_set_output();

    print_header("SUSF Sensor Suite - Raspberry Pi Pico");
    printf("  I2C : SDA=GP%d  SCL=GP%d  100kHz\n", I2C_SDA_PIN, I2C_SCL_PIN);
    printf("  DHT : GP%d\n\n", DHT_PIN);

    /* ── Scan I2C bus ── */
    i2c_scan();
    printf("\n");

    /* ── Set RTC time (only runs if SET_RTC_TIME = 1) ── */
#if SET_RTC_TIME
    ds1307_set_time();
    sleep_ms(100);
#endif

    /* ── Init sensors ── */
    bool mpu_ok = mpu6050_init();
    bool lsm_ok = lsm6dso_init();
    bool lis_ok = lis3mdl_init();
    bool lps_ok = lps22df_init();

    printf("\n  MPU-6050 : %s\n", mpu_ok ? "OK" : "FAIL - is AD0 wired to 3.3V?");
    printf("  LSM6DSO  : %s\n", lsm_ok ? "OK" : "FAIL");
    printf("  LIS3MDL  : %s\n", lis_ok ? "OK" : "FAIL");
    printf("  LPS22DF  : %s\n", lps_ok ? "OK" : "FAIL");
    print_divider('=', 60);
    sleep_ms(1000);

    uint32_t loop = 0;

    while (true) {
        loop++;

        DHT11Data   dht = dht11_read();
        DS1307Data  rtc = ds1307_read();
        MPU6050Data mpu = mpu6050_read();
        LSM6DSOData lsm = lsm6dso_read();
        LIS3MDLData lis = lis3mdl_read();
        LPS22DFData lps = lps22df_read();

        printf("\n\n");
        print_header("SUSF SENSOR READOUT");

        print_section("DS1307  Real-Time Clock  (UK Time)");
        if (rtc.valid) {
            printf("  Date : %s %02u/%02u/%04u\n",
                   day_name(rtc.day), rtc.date, rtc.month, rtc.year);
            printf("  Time : %02u:%02u:%02u\n", rtc.hour, rtc.min, rtc.sec);
        } else {
            printf("  !! DS1307 read failed\n");
        }

        print_section("DHT11  Temperature & Humidity  (GP15)");
        if (dht.valid) {
            printf("  Temperature : %5.1f C\n", dht.temperature);
            printf("  Humidity    : %5.1f %%RH\n", dht.humidity);
        } else {
            printf("  !! DHT11 read failed\n");
        }

        print_section("MPU-6050 / ITG  Accelerometer & Gyroscope");
        if (mpu.valid) {
            printf("  Accel  X:%+7.3fg  Y:%+7.3fg  Z:%+7.3fg\n", mpu.ax, mpu.ay, mpu.az);
            printf("  Gyro   X:%+8.2f   Y:%+8.2f   Z:%+8.2f  deg/s\n", mpu.gx, mpu.gy, mpu.gz);
            printf("  Chip Temp : %5.1f C\n", mpu.temp);
        } else {
            printf("  !! MPU-6050 read failed\n");
        }

        print_section("LSM6DSO  Accel & Gyro  (AltIMU-10 v6)");
        if (lsm.valid) {
            printf("  Accel  X:%+7.3fg  Y:%+7.3fg  Z:%+7.3fg\n", lsm.ax, lsm.ay, lsm.az);
            printf("  Gyro   X:%+8.2f   Y:%+8.2f   Z:%+8.2f  deg/s\n", lsm.gx, lsm.gy, lsm.gz);
        } else {
            printf("  !! LSM6DSO read failed\n");
        }

        print_section("LIS3MDL  Compass / Magnetometer  (AltIMU-10 v6)");
        if (lis.valid) {
            printf("  Mag  X:%+7.4fG  Y:%+7.4fG  Z:%+7.4fG\n", lis.mx, lis.my, lis.mz);
        } else {
            printf("  !! LIS3MDL read failed\n");
        }

        print_section("LPS22DF  Barometric Altimeter  (AltIMU-10 v6)");
        if (lps.valid) {
            printf("  Pressure    : %8.2f hPa\n", lps.pressure);
            printf("  Altitude    : %8.1f m\n",   lps.altitude);
            printf("  Temperature : %8.2f C\n",   lps.temperature);
        } else {
            printf("  !! LPS22DF read failed\n");
        }

        print_divider('=', 60);
        printf("  Loop #%lu  |  Next reading in 2 seconds...\n", loop);
        print_divider('=', 60);

        sleep_ms(2000);
    }

    return 0;
}
