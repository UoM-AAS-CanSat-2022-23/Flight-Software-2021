// this must be included before Arduino.h, because Arduino.h produces colliding macro names
#include <fmt/core.h>
#include <optional>
#include <string>
#include <string_view>

#include <Arduino.h>
#undef B1
#include <TimeLib.h>
#include <Servo.h>

#include "command/parser.hpp"
#include "constants.hpp"
#include "runner.hpp"
#include "sensor/manager.hpp"
#include "telemetry/manager.hpp"
#include "util/sout.hpp"
#include "util/misc.hpp"
#include "xbee/manager.hpp"

XBeeManager xbee_mgr;
SensorManager sensor_mgr {};
TelemetryManager telem_mgr { xbee_mgr, sensor_mgr };
CommandParser cmd_parser { telem_mgr };
Runner runner;
bool tp_released = false;
bool parachute_released = false;
Servo SERVO_PARACHUTE;
Servo SERVO_CONTINUOUS;
Servo SERVO_SPOOL;

void handle_response(Rx16Response&, uintptr_t);
void add_tasks_to_runner();

void setup() {
	// setup pins
	pinMode(BUZZER_PIN, OUTPUT);
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(SERVO_PARACHUTE_PIN, OUTPUT);
	pinMode(SERVO_SPOOL_PIN, OUTPUT);
	pinMode(SERVO_CONTINUOUS_PIN, OUTPUT);

	// setup serial connections / peripherals
	Serial.begin(DEBUG_SERIAL_BAUD);
	xbee_mgr.setup(XBEE_SERIAL);
	xbee_mgr.set_panid(GCS_LINK_PANID);
	xbee_mgr.onRx16Response(handle_response);
	sensor_mgr.setup();
	
	SERVO_PARACHUTE.attach(SERVO_PARACHUTE_PIN, 1000, 2000);
    SERVO_SPOOL.attach(SERVO_SPOOL_PIN, 1000, 2000);
	SERVO_CONTINUOUS.attach(SERVO_CONTINUOUS_PIN, 1350, 1650);

	// setup RTC as time provider
	setSyncProvider(util::getTeensy3Time);

	// add the tasks for the runner to do
	add_tasks_to_runner();
}

void loop() {
	runner.run();
}

// simple function to add tasks to the runner
// in it's own function to make setup clearer
void add_tasks_to_runner() {
	// run the xbee manager's loop every time
	runner.schedule_task(
		[]() { xbee_mgr.loop(); });

	// slow blink LED to show the cansat is on and the main loop is running
	runner.schedule_task(
		1000,
		[]() { digitalToggleFast(LED_BUILTIN); });

	// send the container telemetry once a second if its enabled
	runner.schedule_task(
		1000,
		[]() { telem_mgr.send_container_telemetry(); });

	// send the payload telemetry four times a second if the
	// payload is released
	// imagine having a payload lmao
	// runner.schedule_task(
	// 	250,
	// 	[]() {
	// 		if (!tp_released)
	// 			return;

	// 		xbee_mgr.set_panid(PAYLOAD_LINK_PANID);
	// 		// poll the container for the data, for now just mock it out
	// 		// NOTE: eventually none of this will be needed as the handle_response method
	// 		//       will get called for the response from the XBee
	// 		std::string_view mock_payload_relay_data { "165.2,13.7,5.02,0.18,0.08,-0.18,0.12,0.31,9.8,0.19,-0.05,0.47,12,LANDED" };
	// 		xbee_mgr.set_panid(GCS_LINK_PANID);
	// 		telem_mgr.forward_payload_telemetry(mock_payload_relay_data);
	// 	});
	
	// Release parachute
	runner.schedule_task(
		1000,
		[]() {
			if (parachute_released==true)
				return;
			
			if (sensor_mgr.read_container_telemetry().altitude <= 400){
				parachute_released = true;
				SERVO_PARACHUTE.write(0);
			}
		});

	// Release payload
	runner.schedule_task(
		1000,
		[]() {
			if (tp_released==true)
				return;
			
			if (sensor_mgr.read_container_telemetry().altitude <= 300){
				tp_released = true;
				SERVO_SPOOL.write(160);
				SERVO_CONTINUOUS.write(-180);
				runner.run_after(20'000, []() { SERVO_CONTINUOUS.write(88); });
			}
		});

	// Start Buzzer
	runner.schedule_task(
		1000,
		[]() {
			if (sensor_mgr.read_container_telemetry().altitude <= 20){
				tone(BUZZER_PIN, 1000);
			}
		});

}

// remove the logging messages once we aren't testing any more
struct CommandHandler {
	void operator()(const std::monostate&) {
		// parse failed, ignore it
		sout << "[CommandHandler] Parsing the command failed." << std::endl;
	}

	void operator()(const OnOff& on_off) {
		sout << fmt::format("[CommandHandler] Got ON_OFF value: {}", on_off) << std::endl;
		telem_mgr.set_enabled(on_off);
	}

	void operator()(const UtcTime& utc_time) {
		sout << fmt::format("[CommandHandler] Got UtcTime value: {}", utc_time) << std::endl;

		// we don't care about the full date, just the hour minute and second
		setTime(utc_time.h, utc_time.m, utc_time.s, 0, 0, 0);
	}

	void operator()(const SimulationMode& mode) {
		sout << fmt::format("[CommandHandler] Got MODE value: {:?}", mode) << std::endl;

		sensor_mgr.set_sim_mode(mode);
		sout << fmt::format("[CommandHandler] sim_mode={:?}", sensor_mgr.get_sim_mode()) << std::endl;
	}

	void operator()(const Pressure& pressure) {
		sout << fmt::format("[CommandHandler] Got PRESSURE value: {:.2f}", pressure) << std::endl;
		sensor_mgr.set_sim_pressure(pressure);
	}

	void operator()(const Command& cmd) {
		sout << fmt::format("[CommandHandler] Got Command value: {}", cmd) << std::endl;
		// TODO: implement this with a state
	}

	void operator()(const TetheredPayloadDepth& tpd) {
		sout << fmt::format("[CommandHandler] Got TPD value: {}", tpd) << std::endl;
	}
};

void handle_response(Rx16Response& resp, uintptr_t) {
	// make sure we are listening on the gcs link as much as possible
	if (xbee_mgr.get_panid() != GCS_LINK_PANID) {
		xbee_mgr.set_panid(GCS_LINK_PANID);
	}

	std::string_view data_view {
		reinterpret_cast<char*>(resp.getData()),
		static_cast<size_t>(resp.getDataLength())
	};

	switch (resp.getRemoteAddress16()) {
	case GCS_XBEE_ADDRESS: {
		// data is from the ground station, interpret it as a command
		std::string cmd_string { data_view };
		std::visit(CommandHandler {}, cmd_parser.parse(cmd_string));
		break;
	}
	case PAYLOAD_XBEE_ADDRESS:
		// data is from payload, forward it to the ground station
		telem_mgr.forward_payload_telemetry(data_view);
		break;
	default:
		// invalid PANID, maybe log in future?
		break;
	}
}
