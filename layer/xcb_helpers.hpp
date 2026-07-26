#pragma once

#include <X11/Xlib-xcb.h>
#include <xcb/composite.h>
#include <cstdio>
#include <optional>

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
    return VkRect2D {
      .offset = child.offset,
      .extent = VkExtent2D {
        .width  = std::min<uint32_t>(child.extent.width,  std::max<int32_t>(parent.extent.width  - child.offset.x, 0)),
        .height = std::min<uint32_t>(child.extent.height, std::max<int32_t>(parent.extent.height - child.offset.y, 0)),
      },
    };
  }

  static VkExtent2D max(VkExtent2D a, VkExtent2D b) {
    return VkExtent2D {
      .width  = std::max<uint32_t>(a.width,  b.width),
      .height = std::max<uint32_t>(a.height, b.height),
    };
  }

  // Extent of the overlap of two rects in the same coordinate space.
  // (clip() above assumes the child rect is expressed relative to the
  // parent rect's origin, which doesn't hold for two siblings.)
  static VkExtent2D intersect(VkRect2D a, VkRect2D b) {
    const int32_t x0 = std::max(a.offset.x, b.offset.x);
    const int32_t y0 = std::max(a.offset.y, b.offset.y);
    const int32_t x1 = std::min(a.offset.x + int32_t(a.extent.width),  b.offset.x + int32_t(b.extent.width));
    const int32_t y1 = std::min(a.offset.y + int32_t(a.extent.height), b.offset.y + int32_t(b.extent.height));
    // An intersection that is empty on either axis is empty, full stop --
    // don't report a phantom extent on the other axis.
    if (x1 <= x0 || y1 <= y0)
      return VkExtent2D{};
    return VkExtent2D {
      .width  = uint32_t(x1 - x0),
      .height = uint32_t(y1 - y0),
    };
  }

  static std::optional<VkExtent2D> getLargestObscuringChildWindowSize(xcb_connection_t* connection, xcb_window_t window) {
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
    for (uint32_t i = 0; i < reply->children_len; i++) {
      xcb_window_t child = children[i];

      xcb_get_window_attributes_cookie_t attributeCookie = xcb_get_window_attributes(connection, child);
      auto attributeReply = Reply<xcb_get_window_attributes_reply_t>{ xcb_get_window_attributes_reply(connection, attributeCookie, nullptr) };

      const bool obscuring =
        attributeReply &&
        attributeReply->map_state == XCB_MAP_STATE_VIEWABLE &&
        !attributeReply->override_redirect;

      if (obscuring) {
        if (auto childRect = getWindowRect(connection, child)) {
          VkRect2D clippedRect = clip(*ourRect, *childRect);
          largestExtent = max(largestExtent, clippedRect.extent);
        }
      }
    }

    return largestExtent;
  }

  struct ObscuringSibling {
    xcb_window_t window;
    VkExtent2D overlap;
  };

  // Walks from `window` up to `toplevel`, looking at every level for mapped,
  // non-override-redirect windows OUTSIDE our ancestor chain that overlap it.
  //
  // getLargestObscuringChildWindowSize() only sees the presenting window's
  // own children, but clients like CEF (steamwebhelper) put other content --
  // e.g. Steam's GamepadUI side menus -- in sibling subtrees of the same
  // toplevel. Bypassing XWayland overrides the whole toplevel with just the
  // presenting window's buffer, so anything living in a sibling would be
  // dropped from presentation entirely.
  //
  // Like the child check, unmapped helpers and override-redirect windows
  // don't count: CEF's actual render targets are override-redirect, but they
  // always sit inside a non-override-redirect host window, which is what we
  // key off. X clips children to their parent, so a small host can't leak a
  // larger child either.
  static std::optional<ObscuringSibling> findLargestObscuringSibling(xcb_connection_t* connection, xcb_window_t window, xcb_window_t toplevel) {
    std::optional<ObscuringSibling> largest;

    xcb_window_t current = window;
    // Depth-bounded in case the window is reparented mid-walk and we never
    // meet `toplevel`.
    for (uint32_t depth = 0; current != toplevel && depth < 64; depth++) {
      xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, current);
      auto reply = Reply<xcb_query_tree_reply_t>{ xcb_query_tree_reply(connection, cookie, nullptr) };
      if (!reply || current == reply->root || reply->parent == reply->root)
        break;
      xcb_window_t parent = reply->parent;

      // Our rect and our siblings' rects are all relative to `parent`.
      auto ourRect = getWindowRect(connection, current);
      if (!ourRect)
        break;

      xcb_query_tree_cookie_t parentCookie = xcb_query_tree(connection, parent);
      auto parentReply = Reply<xcb_query_tree_reply_t>{ xcb_query_tree_reply(connection, parentCookie, nullptr) };
      if (!parentReply)
        break;

      xcb_window_t* siblings = xcb_query_tree_children(parentReply.get());
      for (uint32_t i = 0; i < parentReply->children_len; i++) {
        xcb_window_t sibling = siblings[i];
        if (sibling == current)
          continue;

        xcb_get_window_attributes_cookie_t attributeCookie = xcb_get_window_attributes(connection, sibling);
        auto attributeReply = Reply<xcb_get_window_attributes_reply_t>{ xcb_get_window_attributes_reply(connection, attributeCookie, nullptr) };

        const bool obscuring =
          attributeReply &&
          attributeReply->map_state == XCB_MAP_STATE_VIEWABLE &&
          !attributeReply->override_redirect;
        if (!obscuring)
          continue;

        auto siblingRect = getWindowRect(connection, sibling);
        if (!siblingRect)
          continue;

        VkExtent2D overlap = intersect(*ourRect, *siblingRect);
        // Same 1x1 tolerance as the obscuring-children check.
        if (overlap.width <= 1 && overlap.height <= 1)
          continue;

        if (!largest || uint64_t(overlap.width) * overlap.height > uint64_t(largest->overlap.width) * largest->overlap.height)
          largest = ObscuringSibling{ sibling, overlap };
      }

      current = parent;
    }

    return largest;
  }

}

inline int32_t iabs(int32_t a) {
  if (a < 0)
    return -a;

  return a;
}
