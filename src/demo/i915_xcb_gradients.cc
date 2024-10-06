/** See http://litherum.blogspot.com/2014/12/design-of-mesa-3d-part-10-intels-device.html */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <exception>
#include <functional>
#include <string>
#include <map>
#include <system_error>

extern "C" {
#include <xcb/xcb.h>
#include <xcb/dri2.h>
}

#include <llt_gpgpu_rt/i915_runtime.h>
#include "utils.h"
#include "i915_xcb_gradients.clch"

using namespace std;


/* Macros */
#define FINALLY(S,F) { try {S; F;} catch (...) {F; throw;} }


double get_time()
{
	struct timespec t;
	if (clock_gettime(CLOCK_MONOTONIC, &t) < 0)
		throw system_error(errno, generic_category(), "clock_gettime");

	return t.tv_sec + t.tv_nsec * 1e-9;
}


struct drm_buffer_info
{
	uint32_t name;
	uint32_t width;
	uint32_t height;
	uint32_t pitch;

	size_t size() const
	{
		return height * pitch;
	}
};


class XCBConnection final
{
public:
	using window_event_function_t = function<void(const xcb_generic_event_t*)>;

protected:
	xcb_connection_t* conn = nullptr;
	const xcb_screen_t* screen = nullptr;
	xcb_visualid_t visual_id;
	const xcb_query_extension_reply_t* ext_dri2 = nullptr;

	map<xcb_window_t, window_event_function_t> windows;
	map<string, xcb_atom_t> atom_cache;

	uint32_t dri2_version_major = 0;
	uint32_t dri2_version_minor = 0;

	void print_screen_info() const
	{
		printf("Information about screen:\n"
				"    root window:    0x%x\n"
				"    width:          %d\n"
				"    height:         %d\n"
				"    black pixel:    0x%x\n"
				"    white pixel:    0x%x\n"
				"    backing stores: %d\n",
				(int) screen->root,
				(int) screen->width_in_pixels, (int) screen->height_in_pixels,
				(int) screen->black_pixel, (int) screen->white_pixel,
				(int) screen->backing_stores);
	}

	void process_event(const xcb_generic_event_t* e)
	{
		xcb_window_t wid;
		switch (e->response_type & ~0x80)
		{
		case XCB_EXPOSE:
			wid = ((const xcb_expose_event_t*) e)->window;
			break;

		case XCB_CONFIGURE_NOTIFY:
			wid = ((const xcb_configure_notify_event_t*) e)->window;
			break;

		case XCB_CLIENT_MESSAGE:
			wid = ((const xcb_client_message_event_t*) e)->window;
			break;

		default:
			return;
		}

		/* Find window and deliver event */
		auto i = windows.find(wid);
		if (i != windows.end())
			i->second(e);
		else
			fprintf(stderr, "WARNING: Got event for unknown window\n");
	}

public:
	XCBConnection()
	{
		int screen_num;
		conn = xcb_connect(nullptr, &screen_num);
		if (!conn)
			throw runtime_error("Failed to connect to X server");

		try
		{
			/* Retrieve basic information about the server/screen */
			auto i = xcb_setup_roots_iterator(xcb_get_setup(conn));
			for (; i.rem; xcb_screen_next(&i))
			{
				if (screen_num-- == 0)
				{
					screen = i.data;
					break;
				}
			}

			if (!screen)
				throw runtime_error("Failed to get information about screen");

			// print_screen_info();

			/* Initialize extensions */
			{
				ext_dri2 = xcb_get_extension_data(conn, &xcb_dri2_id);
				if (!ext_dri2)
					throw runtime_error("Failed to query DRI2 extension");

				if (!ext_dri2->present)
					throw runtime_error("DRI2 extension not present");

				auto vers = xcb_dri2_query_version(conn,
						XCB_DRI2_MAJOR_VERSION, XCB_DRI2_MINOR_VERSION);

				auto vers_r = xcb_dri2_query_version_reply(conn, vers, nullptr);
				dri2_version_major = vers_r->major_version;
				dri2_version_minor = vers_r->minor_version;
				free(vers_r);

				if (dri2_version_major != 1 || dri2_version_minor < 3)
					throw runtime_error("Unsupported DRI2 version");
			}


			/* Find a TrueColor visual */
			bool visual_found = false;

			{
				auto di = xcb_screen_allowed_depths_iterator(screen);
				for (; di.rem && !visual_found; xcb_depth_next(&di))
				{
					if (di.data->depth != 24)
						continue;

					auto vi = xcb_depth_visuals_iterator(di.data);
					for (; vi.rem && !visual_found; xcb_visualtype_next(&vi))
					{
						auto& v = *(vi.data);

						if (
								v._class == XCB_VISUAL_CLASS_TRUE_COLOR &&
								v.bits_per_rgb_value == 8 &&
								v.red_mask == 0xff0000 &&
								v.green_mask == 0xff00 &&
								v.blue_mask == 0xff)
						{
							visual_id = v.visual_id;
							visual_found = true;
						}
					}
				}
			}

			if (!visual_found)
				throw runtime_error("Failed to find a TrueColor visual");
		}
		catch (...)
		{
			xcb_disconnect(conn);
			throw;
		}
	}

	~XCBConnection()
	{
		xcb_disconnect(conn);
	}

	xcb_connection_t* get_conn()
	{
		return conn;
	}

	const xcb_screen_t* get_screen() const
	{
		return screen;
	}

	xcb_visualid_t get_visual_id() const
	{
		return visual_id;
	}

	xcb_atom_t get_atom(const string& name)
	{
		auto i = atom_cache.find(name);
		if (i != atom_cache.end())
			return i->second;

		auto cookie = xcb_intern_atom(conn, 1, name.size(), name.c_str());
		auto rep = xcb_intern_atom_reply(conn, cookie, nullptr);

		auto a = rep->atom;
		free(rep);

		if (a == XCB_ATOM_NONE)
			throw runtime_error("No such atom");

		atom_cache.emplace(name, a);
		return a;
	}

	void add_window(xcb_window_t wid, window_event_function_t fct)
	{
		if (windows.find(wid) != windows.end())
			throw invalid_argument("Tried to register window multiple times");

		windows.emplace(wid, fct);
	}

	void remove_window(xcb_window_t wid)
	{
		auto i = windows.find(wid);
		if (i == windows.end())
			throw invalid_argument("Tried to remove non-existent window");

		windows.erase(i);
	}

	void flush()
	{
		xcb_flush(conn);
	}

	void main_iteration(bool block)
	{
		if (block)
		{
			auto e = xcb_wait_for_event(conn);
			if (!e)
				throw runtime_error("I/O error during xcb_wait_for_event");

			FINALLY(process_event(e), free(e));
		}
		else
		{
			for (;;)
			{
				auto e = xcb_poll_for_event(conn);
				if (!e)
					break;

				FINALLY(process_event(e), free(e));
			}
		}
	}
};


class XCBWindow final
{
protected:
	XCBConnection& xcb;
	xcb_connection_t* conn;
	xcb_window_t wid;

	int width = 1280;
	int height = 720;
	bool closed = false;

	string dri2_device_name;
	bool drm_connected = false;

	void process_event(const xcb_generic_event_t* e)
	{
		switch (e->response_type & ~0x80)
		{
		case XCB_CONFIGURE_NOTIFY:
			process_event_configure_notify((const xcb_configure_notify_event_t*) e);
			break;

		case XCB_CLIENT_MESSAGE:
			process_event_client_message((const xcb_client_message_event_t*) e);
			break;

		default:
			break;
		}
	}

	void process_event_configure_notify(const xcb_configure_notify_event_t* e)
	{
		if (e->width != width || e->height != height)
		{
			width = e->width;
			height = e->height;
		}
	}

	void process_event_client_message(const xcb_client_message_event_t* e)
	{
		if (e->type == xcb.get_atom("WM_PROTOCOLS"))
		{
			if (e->data.data32[0] == xcb.get_atom("WM_DELETE_WINDOW"))
				closed = true;
		}
	}

	inline void require_drm_connection()
	{
		if (!drm_connected)
			throw runtime_error("DRM device not connected");
	}

public:
	XCBWindow(XCBConnection& xcb, const char* title)
		: xcb(xcb), conn(xcb.get_conn())
	{
		/* Create the window */
		wid = xcb_generate_id(conn);

		uint32_t params[] = {
			XCB_BACKING_STORE_NOT_USEFUL,
			XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
		};

		xcb_create_window(
				conn,
				24,
				wid,
				xcb.get_screen()->root,
				0, 0,
				width, height,
				0,
				XCB_WINDOW_CLASS_INPUT_OUTPUT,
				xcb.get_visual_id(),
				XCB_CW_BACKING_STORE | XCB_CW_EVENT_MASK,
				params);

		try
		{
			auto wm_protocols = xcb.get_atom("WM_DELETE_WINDOW");
			xcb_change_property(conn, XCB_PROP_MODE_REPLACE, wid,
					xcb.get_atom("WM_PROTOCOLS"), XCB_ATOM_ATOM,
					32, 1, &wm_protocols);

			xcb_change_property(conn, XCB_PROP_MODE_REPLACE, wid,
					XCB_ATOM_WM_NAME, XCB_ATOM_STRING,
					8, strlen(title), title);

			xcb_map_window(conn, wid);

			/* Connect to DRM device */
			{
				auto cookie = xcb_dri2_connect(conn, wid,
						XCB_DRI2_DRIVER_TYPE_DRI);

				auto rep = xcb_dri2_connect_reply(conn, cookie, nullptr);
				if (!rep)
					throw runtime_error("Failed to connect to DRM device");

				FINALLY({
					dri2_device_name = string(xcb_dri2_connect_device_name(rep),
							xcb_dri2_connect_device_name_length(rep));
				}, free(rep));
			}

			xcb.add_window(wid,
					bind(&XCBWindow::process_event, this, placeholders::_1));
		}
		catch (...)
		{
			xcb_destroy_window(conn, wid);
		}
	}

	~XCBWindow()
	{
		xcb.remove_window(wid);

		if (drm_connected)
			xcb_dri2_destroy_drawable(conn, wid);

		xcb_destroy_window(conn, wid);
	}

	bool is_closed() const
	{
		return closed;
	}

	string get_drm_device_name() const
	{
		return dri2_device_name;
	}

	void connect_drm_device(drm_magic_t drm_magic)
	{
		if (drm_connected)
			throw runtime_error("Window already connected to DRM");

		auto cookie = xcb_dri2_authenticate(conn, wid, drm_magic);
		auto rep = xcb_dri2_authenticate_reply(conn, cookie, nullptr);
		auto authenticated = rep->authenticated;
		free(rep);

		if (authenticated != 1)
			throw runtime_error("Failed to authenticate with DRM device");

		/* Make window available to DRM */
		xcb_dri2_create_drawable(conn, wid);

		drm_connected = true;
	}

	drm_buffer_info get_backbuffer()
	{
		require_drm_connection();
		drm_buffer_info buf;

		uint32_t attachment = XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT;
		auto cookie = xcb_dri2_get_buffers(conn, wid, 1, 1, &attachment);
		auto rep = xcb_dri2_get_buffers_reply(conn, cookie, nullptr);
		if (!rep)
			throw runtime_error("Failed to get back buffer");

		buf.width = rep->width;
		buf.height = rep->height;

		FINALLY({
			auto i = xcb_dri2_get_buffers_buffers_iterator(rep);

			if (!i.data && i.rem < 1)
				throw runtime_error("No back buffer returned");

			buf.name = i.data->name;
			buf.pitch = i.data->pitch;

			if (i.rem != 1)
				throw runtime_error("Got more than one bo for back buffer");
		}, free(rep));

		return buf;
	}

	void swap_buffers()
	{
		auto cookie = xcb_dri2_swap_buffers(
				conn, wid,
				0, 0,
				0, 0,
				0, 0);

		auto rep = xcb_dri2_swap_buffers_reply(conn, cookie, nullptr);
		if (!rep)
			throw runtime_error("swap buffers failed");

		free(rep);
	}

	int get_width() const
	{
		return width;
	}

	int get_height() const
	{
		return height;
	}

	void set_fullscreen(bool enable)
	{
		xcb_client_message_event_t evt = { 0 };
		evt.response_type = XCB_CLIENT_MESSAGE;
		evt.format = 32;
		evt.window = wid;
		evt.type = xcb.get_atom("_NET_WM_STATE");
		evt.data.data32[0] = enable ? 1 : 0;
		evt.data.data32[1] = xcb.get_atom("_NET_WM_STATE_FULLSCREEN");

		xcb_send_event(
				conn, false, xcb.get_screen()->root,
				XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
				reinterpret_cast<const char*>(&evt));

		xcb.flush();
	}
};


void update_pattern(int width, int height,
		AlignedBuffer& y, AlignedBuffer& cb, AlignedBuffer& cr)
{
	static int y_vals[6]  = {  63,  127,  63,  191,  191, 127 };
	static int cb_vals[6] = {   0, -255, 255, -255,    0, 255 };
	static int cr_vals[6] = { 255, -255,   0,    0, -255, 255 };

	if (width > 3840 || height > 2160)
		return;

	auto y_ptr = (int16_t*) y.ptr();
	auto cb_ptr = (int16_t*) cb.ptr();
	auto cr_ptr = (int16_t*) cr.ptr();

	for (int i = 0; i < height; i++)
	{
		int I = (i * 6) / height;

		for (int j = 0; j < width; j++)
		{
			y_ptr[j] = y_vals[I];
			cb_ptr[j] = cb_vals[I];
			cr_ptr[j] = cr_vals[I];
		}

		y_ptr += 3840;
		cb_ptr += 3840;
		cr_ptr += 3840;
	}
}

void draw(OCL::I915RTE& rte, shared_ptr<OCL::Kernel> kernel, XCBWindow& win,
		double pos, AlignedBuffer& colormap)
{
	auto buf = win.get_backbuffer();

	/* Execute kernel */
	auto pkernel = rte.prepare_kernel(kernel);
	auto& i915_pkernel = static_cast<OCL::I915PreparedKernel&>(*pkernel);

	unsigned x_tiles = DIV_ROUND_UP(buf.width, 128);

	pkernel->add_argument((unsigned) buf.width);
	pkernel->add_argument((unsigned) buf.height);
	pkernel->add_argument((unsigned) buf.pitch / 4);
	pkernel->add_argument((unsigned) (pos * 65535));
	pkernel->add_argument(colormap.ptr(), colormap.size());
	i915_pkernel.add_argument_gem_name(buf.name);
	pkernel->execute(
			OCL::NDRange(x_tiles * 128, ((buf.height + 1) / 2) * 2),
			OCL::NDRange(128, 2));

	/* Swap buffers */
	win.swap_buffers();
}

int main(int argc, char** argv)
{
	bool fullscreen = argc == 2 && strcmp(argv[1], "-f") == 0;

	try
	{
		/* Create XCB connection */
		XCBConnection xcb;

		/* Create XCB window */
		XCBWindow win(xcb, "i915_xcb_gradients");
		if (fullscreen)
			win.set_fullscreen(true);

		xcb.flush();

		/* Initialize GPGUP runtime */
		auto rte = OCL::create_i915_rte(win.get_drm_device_name().c_str());
		win.connect_drm_device(rte->get_drm_magic());
		xcb.flush();

		/* Load kernel */
		auto kernel = rte->read_compiled_kernel(
				CompiledGPUProgramsI915::i915_xcb_gradients(), "test_pattern");

		/* Initialize colormap */
		auto page_size = rte->get_page_size();
		AlignedBuffer colormap(page_size, (8*8 + page_size - 1) & ~(page_size - 1));

		auto ptr = (uint16_t*) colormap.ptr();
		*ptr++ = 0xff; *ptr++ = 0x00; *ptr++ = 0x00; ptr++;
		*ptr++ = 0x00; *ptr++ = 0xff; *ptr++ = 0x00; ptr++;
		*ptr++ = 0x00; *ptr++ = 0x00; *ptr++ = 0xff; ptr++;
		*ptr++ = 0x00; *ptr++ = 0xff; *ptr++ = 0xff; ptr++;
		*ptr++ = 0xff; *ptr++ = 0x00; *ptr++ = 0xff; ptr++;
		*ptr++ = 0xff; *ptr++ = 0xff; *ptr++ = 0x00; ptr++;
		*ptr++ = 0xff; *ptr++ = 0xff; *ptr++ = 0xff; ptr++;
		*ptr++ = 0xff; *ptr++ = 0xff; *ptr++ = 0xff; ptr++;

		/* Main loop */
		while (!win.is_closed())
		{
			auto pos = get_time() / 10;
			pos = pos - floor(pos);

			xcb.main_iteration(false);
			draw(*rte, kernel, win, pos, colormap);
		}
	}
	catch (exception& e)
	{
		fprintf(stderr, "ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
