#pragma once

#include <X11/Xlib-xcb.h>
#include <xcb/composite.h>
#include <xcb/damage.h>
#include <xcb/present.h>
#include <xcb/xfixes.h>
#include <unistd.h>
#include <cstdio>
#include <optional>
#include <utility>
#include <vector>

namespace xcb {

  struct ReplyDeleter {
    template <typename T>
    void operator()(T* ptr) const {
      free(const_cast<std::remove_const_t<T>*>(ptr));
    }
  };

  template <typename T>
  using Reply = std::unique_ptr<T, ReplyDeleter>;

  static std::optional<xcb_atom_t> getAtom(xcb_connection_t* connection, std::string_view name) {
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, false, name.length(), name.data());
    auto reply = Reply<xcb_intern_atom_reply_t>{ xcb_intern_atom_reply(connection, cookie, nullptr) };
    if (!reply) {
      fprintf(stderr, "[Gamescope WSI] Failed to get xcb atom.\n");
      return std::nullopt;
    }
    xcb_atom_t atom = reply->atom;
    return atom;
  }

  template <typename T>
  static std::optional<T> getPropertyValue(xcb_connection_t* connection, xcb_atom_t atom) {
    static_assert(sizeof(T) % 4 == 0);

    xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;

    xcb_get_property_cookie_t cookie = xcb_get_property(connection, false, screen->root, atom, XCB_ATOM_CARDINAL, 0, sizeof(T) / sizeof(uint32_t));
    auto reply = Reply<xcb_get_property_reply_t>{ xcb_get_property_reply(connection, cookie, nullptr) };
    if (!reply) {
      fprintf(stderr, "[Gamescope WSI] Failed to read T root window property.\n");
      return std::nullopt;
    }

    if (reply->type != XCB_ATOM_CARDINAL) {
      fprintf(stderr, "[Gamescope WSI] Atom of T was wrong type. Expected XCB_ATOM_CARDINAL.\n");
      return std::nullopt;
    }

    T value = *reinterpret_cast<const T *>(xcb_get_property_value(reply.get()));
    return value;
  }

  template <typename T>
  static std::optional<T> getPropertyValue(xcb_connection_t* connection, std::string_view name) {
    std::optional<xcb_atom_t> atom = getAtom(connection, name);
    if (!atom)
      return std::nullopt;

    return getPropertyValue<T>(connection, *atom);
  }

  template <typename T>
  static std::optional<T> getPropertyValue(xcb_connection_t* connection, xcb_window_t window, std::string_view name) {
    static_assert(sizeof(T) % 4 == 0);

    std::optional<xcb_atom_t> atom = getAtom(connection, name);
    if (!atom)
      return std::nullopt;

    xcb_get_property_cookie_t cookie = xcb_get_property(connection, false, window, *atom, XCB_ATOM_CARDINAL, 0, sizeof(T) / sizeof(uint32_t));
    auto reply = Reply<xcb_get_property_reply_t>{ xcb_get_property_reply(connection, cookie, nullptr) };
    if (!reply) {
      fprintf(stderr, "[Gamescope WSI] getPropertyValue: xcb_get_property failed for window 0x%x.\n", window);
      return std::nullopt;
    }

    // An absent property is expected, eg. on non-Wine windows.
    if (reply->type != XCB_ATOM_CARDINAL || xcb_get_property_value_length(reply.get()) < int(sizeof(T)))
      return std::nullopt;

    T value = *reinterpret_cast<const T *>(xcb_get_property_value(reply.get()));
    return value;
  }

  static std::optional<xcb_window_t> getParentWindow(xcb_connection_t* connection, xcb_window_t window) {
    xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, window);
    auto reply = Reply<xcb_query_tree_reply_t>{ xcb_query_tree_reply(connection, cookie, nullptr) };
    if (!reply) {
      fprintf(stderr, "[Gamescope WSI] getParentWindow: xcb_query_tree failed for window 0x%x.\n", window);
      return std::nullopt;
    }
    if (reply->root == window || reply->parent == reply->root)
      return std::nullopt;
    return reply->parent;
  }

  static bool isOverrideRedirect(xcb_connection_t* connection, xcb_window_t window) {
    xcb_get_window_attributes_cookie_t cookie = xcb_get_window_attributes(connection, window);
    auto reply = Reply<xcb_get_window_attributes_reply_t>{ xcb_get_window_attributes_reply(connection, cookie, nullptr) };
    if (!reply) {
      fprintf(stderr, "[Gamescope WSI] isOverrideRedirect: xcb_get_window_attributes failed for window 0x%x.\n", window);
      return false;
    }
    return reply->override_redirect;
  }

  static bool hasProperty(xcb_connection_t* connection, xcb_window_t window, xcb_atom_t atom) {
    xcb_get_property_cookie_t cookie = xcb_get_property(connection, false, window, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 0);
    auto reply = Reply<xcb_get_property_reply_t>{ xcb_get_property_reply(connection, cookie, nullptr) };
    if (!reply) {
      fprintf(stderr, "[Gamescope WSI] hasProperty: xcb_get_property failed for window 0x%x.\n", window);
      return false;
    }
    return reply->type != XCB_NONE;
  }

  static std::optional<xcb_window_t> getToplevelWindow(xcb_connection_t* connection, xcb_window_t window) {
    for (;;) {
      xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, window);
      auto reply = Reply<xcb_query_tree_reply_t>{ xcb_query_tree_reply(connection, cookie, nullptr) };

      if (!reply) {
        fprintf(stderr, "[Gamescope WSI] getToplevelWindow: xcb_query_tree failed for window 0x%x.\n", window);
        return std::nullopt;
      }

      if (reply->root == reply->parent)
        return window;

      window = reply->parent;
    }
  }

  static std::optional<VkRect2D> getWindowRect(xcb_connection_t* connection, xcb_window_t window) {
    xcb_get_geometry_cookie_t cookie = xcb_get_geometry(connection, window);
    auto reply = Reply<xcb_get_geometry_reply_t>{ xcb_get_geometry_reply(connection, cookie, nullptr) };
    if (!reply) {
      fprintf(stderr, "[Gamescope WSI] getWindowRect: xcb_get_geometry failed for window 0x%x.\n", window);
      return std::nullopt;
    }

    VkRect2D rect = {
      .offset = { reply->x, reply->y },
      .extent = { reply->width, reply->height },
    };

    return rect;
  }

  static VkRect2D clip(VkRect2D parent, VkRect2D child) {
    const int32_t x0 = std::max<int32_t>(child.offset.x, 0);
    const int32_t y0 = std::max<int32_t>(child.offset.y, 0);
    const int32_t x1 = std::min<int32_t>(child.offset.x + int32_t(child.extent.width),  parent.extent.width);
    const int32_t y1 = std::min<int32_t>(child.offset.y + int32_t(child.extent.height), parent.extent.height);
    if (x1 <= x0 || y1 <= y0)
      return VkRect2D {};

    return VkRect2D {
      .offset = { x0, y0 },
      .extent = { uint32_t(x1 - x0), uint32_t(y1 - y0) },
    };
  }

  static VkExtent2D max(VkExtent2D a, VkExtent2D b) {
    return VkExtent2D {
      .width  = std::max<uint32_t>(a.width,  b.width),
      .height = std::max<uint32_t>(a.height, b.height),
    };
  }

  // `except` is a child to leave out.
  static std::optional<VkExtent2D> getLargestObscuringChildWindowSize(xcb_connection_t* connection, xcb_window_t window, xcb_window_t except = XCB_WINDOW_NONE) {
    VkExtent2D largestExtent = {};

    xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, window);
    auto reply = Reply<xcb_query_tree_reply_t>{ xcb_query_tree_reply(connection, cookie, nullptr) };

    if (!reply) {
      fprintf(stderr, "[Gamescope WSI] getLargestObscuringWindowSize: xcb_query_tree failed for window 0x%x.\n", window);
      return std::nullopt;
    }

    auto ourRect = getWindowRect(connection, window);
    if (!ourRect) {
      fprintf(stderr, "[Gamescope WSI] getLargestObscuringWindowSize: getWindowRect failed for main window 0x%x.\n", window);
      return std::nullopt;
    }

    xcb_window_t* children = xcb_query_tree_children(reply.get());

    // Send every request before reading a reply so we don't round trip per window.
    std::vector<std::pair<xcb_get_window_attributes_cookie_t, xcb_get_geometry_cookie_t>> cookies;
    for (uint32_t i = 0; i < reply->children_len; i++) {
      if (children[i] != except)
        cookies.emplace_back(xcb_get_window_attributes(connection, children[i]), xcb_get_geometry(connection, children[i]));
    }

    for (auto [attributeCookie, geometryCookie] : cookies) {
      auto attributeReply = Reply<xcb_get_window_attributes_reply_t>{ xcb_get_window_attributes_reply(connection, attributeCookie, nullptr) };
      auto geometryReply = Reply<xcb_get_geometry_reply_t>{ xcb_get_geometry_reply(connection, geometryCookie, nullptr) };

      const bool obscuring =
        attributeReply &&
        geometryReply &&
        attributeReply->map_state == XCB_MAP_STATE_VIEWABLE &&
        attributeReply->_class == XCB_WINDOW_CLASS_INPUT_OUTPUT &&
        !attributeReply->override_redirect;

      if (obscuring) {
        // x and y are the border's outer corner, the size is the interior.
        const uint32_t border = 2 * geometryReply->border_width;
        VkRect2D childRect = {
          .offset = { geometryReply->x, geometryReply->y },
          .extent = { geometryReply->width + border, geometryReply->height + border },
        };
        VkRect2D clippedRect = clip(*ourRect, childRect);
        largestExtent = max(largestExtent, clippedRect.extent);
      }
    }

    return largestExtent;
  }

  static std::optional<VkExtent2D> getLargestObscuringSiblingWindowSize(xcb_connection_t* connection, xcb_window_t window) {
    VkExtent2D largestExtent = {};

    while (auto parent = getParentWindow(connection, window)) {
      auto extent = getLargestObscuringChildWindowSize(connection, *parent, window);
      if (!extent)
        return std::nullopt;

      largestExtent = max(largestExtent, *extent);
      window = *parent;
    }

    return largestExtent;
  }

  // Checked, so an error comes back to us and not to the client's error handler.
  static bool check(xcb_connection_t* connection, xcb_void_cookie_t cookie) {
    xcb_generic_error_t* error = xcb_request_check(connection, cookie);
    free(error);
    return !error;
  }

  // Has the client redraw a window whose contents we can't provide.
  static void exposeWindow(xcb_connection_t* connection, xcb_window_t window) {
    check(connection, xcb_clear_area_checked(connection, true, window, 0, 0, 0, 0));
  }

  // Has Xwayland commit the window again without anything being drawn.
  static void damageWindow(xcb_connection_t* connection, xcb_window_t window, VkExtent2D extent) {
    xcb_discard_reply(connection, xcb_xfixes_query_version(connection, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION).sequence);
    xcb_discard_reply(connection, xcb_damage_query_version(connection, XCB_DAMAGE_MAJOR_VERSION, XCB_DAMAGE_MINOR_VERSION).sequence);

    const xcb_rectangle_t rect = { 0, 0, uint16_t(extent.width), uint16_t(extent.height) };
    const xcb_xfixes_region_t region = xcb_generate_id(connection);
    if (check(connection, xcb_xfixes_create_region_checked(connection, region, 1, &rect))) {
      check(connection, xcb_damage_add_checked(connection, window, region));
      check(connection, xcb_xfixes_destroy_region_checked(connection, region));
    }
  }

  // Waits for Xwayland to carry out a present of a pixmap to the window. The
  // requests for it are made after `listen` and before `wait`.
  struct PresentListener {
    xcb_connection_t* connection = nullptr;
    xcb_present_event_t eventId = XCB_NONE;
    xcb_special_event_t* events = nullptr;

    void listen(xcb_connection_t* connection_, xcb_window_t window) {
      stop();
      connection = connection_;
      eventId = xcb_generate_id(connection);
      if (!check(connection, xcb_present_select_input_checked(connection, eventId, window, XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY))) {
        eventId = XCB_NONE;
        return;
      }
      events = xcb_register_for_special_xge(connection, &xcb_present_id, eventId, nullptr);
    }

    bool listening() const {
      return events != nullptr;
    }

    // A client that died mid present never completes it, so this gives up after 100ms.
    bool wait() {
      bool completed = false;
      for (uint32_t i = 0; events && !completed && i < 200; i++) {
        while (xcb_generic_event_t* event = xcb_poll_for_special_event(connection, events)) {
          auto* presentEvent = reinterpret_cast<xcb_present_generic_event_t*>(event);
          if (presentEvent->evtype == XCB_PRESENT_COMPLETE_NOTIFY &&
              reinterpret_cast<xcb_present_complete_notify_event_t*>(event)->kind == XCB_PRESENT_COMPLETE_KIND_PIXMAP)
            completed = true;
          free(event);
        }
        if (!completed)
          usleep(500);
      }
      stop();
      return completed;
    }

    void stop() {
      if (events) {
        xcb_unregister_for_special_event(connection, events);
        events = nullptr;
      }
      eventId = XCB_NONE;
    }
  };

}

inline int32_t iabs(int32_t a) {
  if (a < 0)
    return -a;

  return a;
}
