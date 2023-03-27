/** See http://litherum.blogspot.com/2014/12/design-of-mesa-3d-part-10-intels-device.html */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <string>
#include <map>

extern "C" {
#include <xcb/xcb.h>
#include <xcb/dri2.h>
}

#include "i915_runtime.h"
#include "utils.h"

using namespace std;

const char* kernel_src = R"KERNELSRC(
void __kernel fill_drawable(uint size, uint val, __global uint* dst)
{
	uint i = get_global_id(0);
	if (i < size)
		dst[i] = val;
}

void __kernel test_pattern(uint width, uint height, uint pitch, __global uint* vals, __global uint* dst)
{
	uint I = get_global_id(0);
	uint J = get_global_id(1);

	uint i = get_group_id(0);
	uint ii = get_local_id(0);

	if (I < width && J < height)
	{
		uint y_offset = (J / 8) * pitch * 8 + (J % 8) * 128;
		dst[ii + i*1024 + y_offset] = vals[(I * 6) / width];
	}
}

void __kernel display_irct(
	uint width, uint height, uint dst_pitch, uint src_pitch,
	__global short* src_y, __global short* src_cb, __global short* src_cr,
	__global uint* dst)
{
	uint I = get_global_id(0);
	uint J = get_global_id(1);

	uint i = get_group_id(0);
	uint ii = get_local_id(0);

	if (I < width && J < height)
	{
		int src_offset = J*src_pitch + I;
		int y = src_y[src_offset];
		int cb = src_cb[src_offset];
		int cr = src_cr[src_offset];

		int g = y - ((cb + cr) >> 2);
		int b = g + cb;
		int r = g + cr;

		r <<= 16;
		g <<= 8;

		uint y_offset = (J / 8) * dst_pitch * 8 + (J % 8) * 128;
		dst[ii + i*1024 + y_offset] = r | g | b;
	}
}
)KERNELSRC";


/* Macros */
#define FINALLY(S,F) { try {S; F;} catch (...) {F; throw;} }


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

	int width = 3840;
	int height = 2160;
	bool closed = false;

	string dri2_driver_name;
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
					dri2_driver_name = string(xcb_dri2_connect_driver_name(rep),
							xcb_dri2_connect_driver_name_length(rep));

					dri2_device_name = string(xcb_dri2_connect_device_name(rep),
							xcb_dri2_connect_device_name_length(rep));
				}, free(rep));

				if (dri2_driver_name != "i965")
					throw runtime_error("Unsupported DRI2 driver");
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
		AlignedBuffer& y, AlignedBuffer& cb, AlignedBuffer& cr)
{
	if (win.get_width() > 3840 || win.get_height() > 2160)
		return;

	auto buf = win.get_backbuffer();

	/* Execute kernel */
	auto pkernel = rte.prepare_kernel(kernel);
	auto& i915_pkernel = static_cast<OCL::I915PreparedKernel&>(*pkernel);

	unsigned x_tiles = DIV_ROUND_UP(buf.width, 128);

	pkernel->add_argument((unsigned) buf.width);
	pkernel->add_argument((unsigned) buf.height);
	pkernel->add_argument((unsigned) buf.pitch / 4);
	pkernel->add_argument((unsigned) 3840);
	pkernel->add_argument(y.ptr(), y.size());
	pkernel->add_argument(cb.ptr(), cb.size());
	pkernel->add_argument(cr.ptr(), cr.size());
	i915_pkernel.add_argument_gem_name(buf.name);
	pkernel->execute(
			OCL::NDRange(x_tiles * 128, ((buf.height + 1) / 2) * 2),
			OCL::NDRange(128, 2));

	/* Swap buffers */
	win.swap_buffers();
}

int main(int argc, char** argv)
{
	try
	{
		/* Create XCB connection */
		XCBConnection xcb;

		/* Create XCB window */
		XCBWindow win(xcb, "i915_xcb_display");
		xcb.flush();

		/* Initialize GPGUP runtime */
		auto rte = OCL::create_i915_rte(win.get_drm_device_name().c_str());
		win.connect_drm_device(rte->get_drm_magic());
		xcb.flush();

		/* Compile kernel */
		auto kernel = rte->compile_kernel(kernel_src, "display_irct", "-cl-std=CL1.2");

		auto build_log = kernel->get_build_log();
		if (build_log.size() > 0)
			printf("Build log:\n%s\n", build_log.c_str());


		const size_t src_size = 2 * 3840 * 2160;
		AlignedBuffer y(rte->get_page_size(), src_size);
		AlignedBuffer cb(rte->get_page_size(), src_size);
		AlignedBuffer cr(rte->get_page_size(), src_size);

		/* Main loop */
		int last_width = 0, last_height = 0;

		while (!win.is_closed())
		{
			auto cur_width = win.get_width(), cur_height = win.get_height();
			if (last_width != cur_width || last_height != cur_height)
			{
				last_width = cur_width;
				last_height = cur_height;

				update_pattern(cur_width, cur_height, y, cb, cr);
			}

			xcb.main_iteration(false);
			draw(*rte, kernel, win, y, cb, cr);
		}
	}
	catch (exception& e)
	{
		fprintf(stderr, "ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
