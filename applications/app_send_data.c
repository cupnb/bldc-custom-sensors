/*
	Copyright 2019 Benjamin Vedder	benjamin@vedder.se

	This file is part of the VESC firmware.

	The VESC firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The VESC firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

#include "app.h"
#include "ch.h"
#include "hal.h"

// Some useful includes
#include "mc_interface.h"
#include "utils_math.h"
#include "encoder/encoder.h"
#include "terminal.h"
#include "comm_can.h"
#include "hw.h"
#include "commands.h"
#include "timeout.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

// Threads
static THD_FUNCTION(my_thread, arg);
static THD_WORKING_AREA(my_thread_wa, 1024);

// Private functions
static void pwm_callback(void);
static void terminal_command_calibrate(int argc, const char **argv);

// Private variables
static volatile bool stop_now = true;
static volatile bool is_running = false;

// Calibration values

volatile float rpm_calibration = 0.93;
volatile float dummy = 0.0;

// Called when the custom application is started. Start our
// threads here and set up callbacks.
void app_custom_start(void) {
	mc_interface_set_pwm_callback(pwm_callback);

	stop_now = false;
	chThdCreateStatic(my_thread_wa, sizeof(my_thread_wa),
			NORMALPRIO, my_thread, NULL);

	// Terminal commands for the VESC Tool terminal can be registered.
	terminal_register_command_callback(
			"calibrate",
			"Set the calibration values in this order:",
			"[rpm]",
			terminal_command_calibrate
			);
}

// Called when the custom application is stopped. Stop our threads
// and release callbacks.
void app_custom_stop(void) {
	mc_interface_set_pwm_callback(0);
	terminal_unregister_callback(terminal_command_calibrate);

	stop_now = true;
	while (is_running) {
		chThdSleepMilliseconds(1);
	}
}

void app_custom_configure(app_configuration *conf) {
	(void)conf;
}

static THD_FUNCTION(my_thread, arg) {
	(void)arg;

	chRegSetThreadName("App Custom");

	is_running = true;

	// Example of using the experiment plot
	chThdSleepMilliseconds(8000);
	commands_init_plot("Sample", "Value");
	commands_plot_add_graph("ADC1 (V)");
	commands_plot_add_graph("ADC2 (V)");
	commands_plot_add_graph("RPM (1/s)");
   	commands_plot_add_graph("Current In (A)");
    commands_plot_add_graph("Current In (filtered) (A)");

	float samp = 0.0;
	float rpm = 0.0;

	commands_printf("Starting custom data send app! Compile Time: %s %s\n", __DATE__, __TIME__);
	/*for(;;) {
		commands_plot_set_graph(0);
		commands_send_plot_points(samp, mc_interface_temp_fet_filtered());
		commands_plot_set_graph(1);
		commands_send_plot_points(samp, GET_INPUT_VOLTAGE());
		samp++;
		chThdSleepMilliseconds(10);
	}*/

	for(;;) {
		// Check if it is time to stop.
		if (stop_now) {
			is_running = false;
			return;
		}

		timeout_reset(); // Reset timeout if everything is OK.

		// Run your logic here. A lot of functionality is available in mc_interface.h.

		// Calculate the calibrated values
		rpm = (mc_interface_get_rpm() * rpm_calibration) / 14.0;

		// Push them to VESC Tool
		commands_plot_set_graph(0); //ADC1
		commands_send_plot_points(samp, (float)ADC_VOLTS(ADC_IND_EXT));
		commands_plot_set_graph(1); //ADC2
		commands_send_plot_points(samp, (float)ADC_VOLTS(ADC_IND_EXT2));
		commands_plot_set_graph(2);
		commands_send_plot_points(samp, rpm);
        commands_plot_set_graph(3);
        commands_send_plot_points(samp, mc_interface_get_tot_current_in());
        commands_plot_set_graph(4);
        commands_send_plot_points(samp, mc_interface_get_tot_current_in_filtered());
		samp++;
		
		chThdSleepMilliseconds(10);
	}
}

static void pwm_callback(void) {
	// Called for every control iteration in interrupt context.
}

// Allows to set the calibration variables via the VESC terminal
static void terminal_command_calibrate(int argc, const char **argv) {

	float temp = 0.0;
	int d = 0;

	if (argc == 1) {commands_printf("Please provide arguments\n"); return;}

	if (argc > 1){
		// Get RPM calibration data
		if (sscanf(argv[1], "%f", &temp) == 0){
			commands_printf("Failed to parse argument 1!\n");
			return;
		}

		commands_printf("Argument 1 sscanf: %f", (double)temp);

		if (temp > -10){
			rpm_calibration = temp;
		}
	}

	if (argc > 2){
		// Template for any other arguments in the future
		if ((d = sscanf(argv[2], "%f", &temp)) == 0){
				commands_printf("Failed to parse argument 2!\n");
				return;
		}

		commands_printf("Argument 2 sscanf: %f", (double)temp);
			

		if (temp > -10){
			dummy = temp;
		}
	}

	commands_printf("Command executed successfully, %d arguments parsed!\n\nrpm_calibration = %f\ndummy = %f\n", argc - 1, (double)rpm_calibration, (double)dummy);
	
}