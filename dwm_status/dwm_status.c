/* Compile with -lX11 and -lasound flags */
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>
#include <alsa/asoundlib.h>
#include <alsa/control.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <X11/Xlib.h>

#include "dwm_status.h"

/* Macros */
#define TRUE 1
#define FALSE 0
#define MAX_OUTPUT 512
#define MAX_DATE_OUTPUT 64
#define MAX_VOLUME_OUTPUT 8
#define MAX_WIFI_OUTPUT 16
#define MAX_MEMORY_OUTPUT 8
#define MAX_CPU_OUTPUT 8
#define MAX_SONG_OUTPUT 64
#define MAX_TIME_OUTPUT 8
#define MAX_BUFFER_SIZE 1024

/* Global variables */
static Display      *display;
static const char   *battery_icon;
static const char   *date_icon;
static const char   *time_icon;
static const char   *volume_icon;
static const char   *wifi_icon;
static const char   *memory_icon;
static const char   *cpu_icon;
static const char   *disk_icon;
static float        battery_output;
static char         date_output[MAX_DATE_OUTPUT];
static char         time_output[MAX_DATE_OUTPUT];
static char         song_output[MAX_SONG_OUTPUT];
static char         status_output[MAX_OUTPUT];
static char         volume_output[MAX_VOLUME_OUTPUT];
static char         wifi_output[MAX_WIFI_OUTPUT];
static char         memory_output[MAX_MEMORY_OUTPUT];
static char         cpu_output[MAX_CPU_OUTPUT];
static char         disk_space_output[MAX_CPU_OUTPUT];

int main(void)
{
    display = XOpenDisplay(NULL);
    date_icon = DATE_ICON;
    wifi_icon = WIFI_ICON;
    memory_icon = MEMORY_ICON;
    cpu_icon = CPU_ICON;
    disk_icon = DISK_ICON;

infinite_loop:
    set_time_icon();
    set_volume_and_icon();
    set_wifi_output();
    set_memory_output();
    set_cpu_output();
    set_song_output(song_output);
    set_date(DATE_FORMAT, date_output);
    set_date(TIME_FORMAT, time_output);
    battery_output = get_batteries();
    set_battery_icon();

    snprintf(status_output, MAX_OUTPUT,
             "%s %s %s %s  %s %s  %s %s  %s %.0f%%  %s %s  %s%s",
             song_output,
             wifi_output,
             cpu_icon, cpu_output,
             memory_icon, memory_output,
             volume_icon, volume_output,
             battery_icon, battery_output,
             date_icon, date_output,
             time_icon, time_output);

    set_status();
    nanosleep(&SLEEP_TIME, NULL);
    goto infinite_loop;
}

void set_date(const char *date_format, char date_output[MAX_DATE_OUTPUT])
{
    time_t current_time = time(NULL);

    strftime(date_output, MAX_DATE_OUTPUT,
             date_format, localtime(&current_time));
}

float get_batteries(void)
{
    static float bat0, bat1;

    bat0 = get_battery(ENERGY_NOW_FILE_BAT0, ENERGY_FULL_FILE_BAT0);
    bat1 = get_battery(ENERGY_NOW_FILE_BAT1, ENERGY_FULL_FILE_BAT1);

    return (bat0 + bat1) / 2;
}

float get_battery(const char *energy_now_file, const char *energy_full_file)
{
    static float energy_now, energy_full;
    static FILE *fd;

    fd = fopen(energy_full_file, "r");
    if (fd == NULL)
    {
        return -1;
    }
    fscanf(fd, "%f", &energy_full);
    fclose(fd);

    fd = fopen(energy_now_file, "r");
    if (fd == NULL)
    {
        return -1;
    }
    fscanf(fd, "%f", &energy_now);
    fclose(fd);

    return (energy_now / energy_full) * 100;
}

void set_memory_output(void)
{
    static float memory_used, total_memory;
    static struct sysinfo sys_info;

    sysinfo(&sys_info);
    memory_used = (sys_info.totalram - sys_info.freeram) / GIGABYTE;
    total_memory = sys_info.totalram / GIGABYTE;

    sprintf(memory_output, "%i/%iG",
            (int)(memory_used + 0.5),
            (int)(total_memory + 0.5));
}

void set_cpu_output(void)
{
    static long int prev_load[7] = {}, curr_load[7] = {},
                    curr_load_sum, prev_load_sum, load_delta,
                    idle_time_delta;
    static float cpu_usage;
    static int i;
    static FILE *fd;

    fd = fopen("/proc/stat","r");
    if (fd == NULL)
    {
        return;
    }
    /* Ignore first column which only contains the string 'cpu' */
    /* and read the rest of the cpu load information */
    fscanf(fd, "%*s %li %li %li %li %li %li %li",
           &curr_load[0], &curr_load[1],
           &curr_load[2], &curr_load[3],
           &curr_load[4], &curr_load[5],
           &curr_load[6]);
    fclose(fd);

    for (i = 0, prev_load_sum = curr_load_sum, curr_load_sum = 0; i < 7; i++)
    {
        curr_load_sum += curr_load[i];
    }

    load_delta = prev_load_sum - curr_load_sum;
    load_delta = load_delta < 0 ? -load_delta : load_delta;

    idle_time_delta = curr_load[3] - prev_load[3];
    idle_time_delta = idle_time_delta < 0 ? -idle_time_delta : idle_time_delta;

    cpu_usage = 100 * (load_delta - idle_time_delta) / (float)load_delta;

    for (int i = 0; i < 7; i++)
    {
        prev_load[i] = curr_load[i];
    }

    sprintf(cpu_output, "%.1f%%", cpu_usage);
}

void set_disk_space_output(void)
{
    static unsigned long total_bytes, used_bytes;
    static struct statvfs disk_info;

    if (statvfs("/", &disk_info) < 0)
    {
        return;
    }
    total_bytes = disk_info.f_blocks * disk_info.f_bsize;
    used_bytes = total_bytes - (disk_info.f_bfree * disk_info.f_bsize);

    sprintf(disk_space_output, "%i/%iG",
            (int)(used_bytes / GIGABYTE + 0.5),
            (int)(total_bytes / GIGABYTE + 0.5));
}

void set_volume_and_icon(void)
{
    /* For more detail and documentation on what's involved here visit: */
    /* https://www.alsa-project.org/alsa-doc/alsa-lib/group___simple_mixer.html */
    static const snd_mixer_selem_channel_id_t mixer_channel = SND_MIXER_SCHN_MONO;
    static const char *sound_card = "default";
    static const char *selem_name = "Master";
    static snd_mixer_t *handle;
    static snd_mixer_selem_id_t *sid;
    static snd_mixer_elem_t* elem;
    static long min, max, volume;
    static int playback_active;

    snd_mixer_open(&handle, 0);

    /* Setup handle */
    snd_mixer_attach(handle, sound_card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    /* Setup sid */
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);

    /* Get elem */
    elem = snd_mixer_find_selem(handle, sid);

    /* Get desired values */
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_get_playback_volume(elem, 0, &volume);
    snd_mixer_selem_get_playback_switch(elem, mixer_channel, &playback_active);

    snd_mixer_close(handle);

    /* Convert value to percentage appropriate for output */
    volume = (double)volume / max * 100 + 0.5;
    sprintf(volume_output, "%i%%", (int)volume);

    volume_icon = playback_active == TRUE ? SPEAKER_ICON_UNMUTED :
                                            SPEAKER_ICON_MUTED;
}

void set_wifi_output(void)
{
    static char wifi_data[MAX_BUFFER_SIZE], *wifi_data_ptr;
    static int wifi_strength, i, j;
    static FILE *fd;

    fd = fopen("/proc/net/wireless", "r");
    if (fd == NULL)
    {
        return;
    }
    /* Desired wireless interface data exists on the 3rd line of output */
    for (i = 0; i < 3; i++)
    {
        wifi_data_ptr = fgets(wifi_data, MAX_BUFFER_SIZE, fd);
    }
    fclose(fd);
    /* If the pointer is null it's likely because there is no */
    /* active connection. As a result we'd like to output nothing */
    if (wifi_data_ptr == NULL)
    {
        wifi_output[0] = '\0';
        return;
    }

    /* The first '.' indicates the end of the
     * link quality value which is what we want */
    for (i = 0; wifi_data[i] != '.'; i++);

    /* Step backwards to the beginning of the value */
    for (i--; wifi_data[i] != ' '; i--);

    /* Insert link quality value characters at the beginning of our buffer */
    /* and append null terminator so we can convert to an integer */
    for (j = 0, i++; wifi_data[i] != '.'; i++, j++)
    {
        wifi_data[j] = wifi_data[i];
    }
    wifi_data[j] = '\0';

    /* Convert to a meaningful percentage value appropriate for output */
    wifi_strength = atoi(wifi_data) * 100 / 70;

    sprintf(wifi_output, " %s %i%% ", wifi_icon, wifi_strength);
}

void set_battery_icon(void)
{
    switch ((int)(battery_output + 0.5))
    {
        case 90 ... 100:
            battery_icon = BATTERY_ICON_FULL;
            break;
        case 60 ... 89:
            battery_icon = BATTERY_ICON_THREE_QUARTERS;
            break;
        case 30 ... 59:
            battery_icon = BATTERY_ICON_HALF;
            break;
        case 10 ... 29:
            battery_icon = BATTERY_ICON_QUARTER;
            break;
        default:
            battery_icon = BATTERY_ICON_EMPTY;
            break;
    }
}

void set_time_icon(void)
{
    /* Provide consistent spacing between the time icon and time value */
    /* for both single and double digit times throughout the day. */
    /* All double digit times are prefixed with an empty space. */
    time_icon = time_output[0] != ' ' ? TIME_ICON_WITH_SPACE :
                                        TIME_ICON;
}

void set_status(void)
{
    /* Equivalent to xsetroot -name $(status_output) in bash */
    XStoreName(display, DefaultRootWindow(display), status_output);
    XSync(display, False);
}
