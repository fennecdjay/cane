#include <iostream>
#include <string>
#include <string_view>
#include <chrono>
#include <thread>

#include <report.hpp>
#include <view.hpp>
#include <lib.hpp>

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>


int main(int, const char*[]) {
	std::string device = "j2a";

	try {
		std::istreambuf_iterator<char> begin { std::cin }, end;
		std::string in { begin, end };

		cane::View src { &*in.begin(), &*in.end() };
		cane::Lexer lx { src };

		if (not cane::utf_validate(src))
			lx.error(cane::Phases::ENCODING, src, cane::STR_ENCODING);

		namespace time = std::chrono;

		using clock = time::steady_clock;
		using unit = time::microseconds;

		auto t1 = clock::now();
		cane::Context ctx = cane::compile(lx);
		auto t2 = clock::now();

		time::duration<double, std::micro> t = t2 - t1;
		CANE_LOG(cane::LOG_SUCC, CANE_ANSI_FG_YELLOW "took: {}µs" CANE_ANSI_RESET, t.count());

		if (ctx.timeline.empty())
			return 0;


		// Setup JACK
		using namespace std::chrono_literals;

		struct JackData {
			jack_client_t* client = nullptr;
			jack_port_t* port = nullptr;
			std::vector<cane::Event> events;

			~JackData() {
				jack_client_close(client);
			}
		};

		// Connect to JACK, register a port and register our callback.
		JackData midi {};

		if (not (midi.client = jack_client_open(cane::CSTR_EXE, JackOptions::JackNullOption, nullptr)))
			cane::general_error(cane::STR_MIDI_CONNECT_ERROR);

		if (not (midi.port = jack_port_register(midi.client, cane::CSTR_PORT, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0)))
			cane::general_error(cane::STR_MIDI_PORT_ERROR);

		jack_set_process_callback(midi.client, [] (jack_nframes_t nframes, void *arg) {
			auto& [client, port, events] = *static_cast<JackData*>(arg);

			void* out_buffer = jack_port_get_buffer(port, nframes);
			jack_midi_clear_buffer(out_buffer);

			uint8_t* buffer = nullptr;

			// Copy every MIDI event into the buffer provided by JACK.
			for (cane::Event& ev: events) {
				std::array msg = ev.message();

				if (not (buffer = jack_midi_event_reserve(out_buffer, 0, msg.size())))
					cane::general_error(cane::STR_MIDI_WRITE_ERROR);

				std::memcpy(buffer, msg.data(), msg.size());
			}

			events.clear();  // important so that we don't leak memory

			return 0;
		}, static_cast<void*>(&midi));

		if (jack_activate(midi.client))
			cane::general_error(cane::STR_MIDI_ACTIVATE_ERROR);


		// Get an array of all MIDI input ports that we could potentially connect to.
		const char** ports = nullptr;

		if (not (ports = jack_get_ports(midi.client, device.c_str(), JACK_DEFAULT_MIDI_TYPE, JackPortIsInput)))
			cane::general_error(cane::STR_MIDI_GET_PORTS_ERROR);

		if (*ports == nullptr)  // No MIDI input ports.
			cane::general_error(cane::STR_MIDI_NOT_FOUND, device);

		cane::general_notice(cane::STR_MIDI_FOUND, *ports);

		if (jack_connect(midi.client, jack_port_name(midi.port), *ports))
			cane::general_error(cane::STR_MIDI_PATCH_ERROR);

		jack_free(ports);


		// Sequencer
		size_t dt = 0;
		auto it = ctx.timeline.begin();

		while (true) {
			// Gather all events we need to send now.
			for (; it != ctx.timeline.end() and it->time <= dt; ++it)
				midi.events.emplace_back(*it);

			if (it == ctx.timeline.end())
				break;

			// Wait until the next event.
			// We wait for successively shorter times until we apprach the
			// target until we end up in a busy loop to make sure we keep
			// latency from the OS scheduler to a minimum while also not
			// turning the CPU into a glorified heater.
			auto slpt = it->time - dt;
			auto target = std::chrono::steady_clock::now() + std::chrono::milliseconds { slpt };

			while (std::chrono::steady_clock::now() < target) {
				slpt /= 2;

				if (slpt < 10)
					continue;

				std::this_thread::sleep_for(std::chrono::milliseconds { slpt });
			}

			dt += (it->time - dt);
		}
	}

	catch (cane::Error) {
		return 1;
	}

	return 0;
}
