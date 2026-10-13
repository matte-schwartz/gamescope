#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// Resolves a native window's surface tree into layers.
// Positions are in the root surface's logical coordinates.
namespace gamescope::SurfaceTree
{
	struct Vec2_t
	{
		float x = 0.0f;
		float y = 0.0f;
	};

	struct Box_t
	{
		int32_t x = 0;
		int32_t y = 0;
		int32_t w = 0;
		int32_t h = 0;

		bool Contains( const Box_t &other ) const
		{
			return other.x >= x && other.y >= y &&
				other.x + other.w <= x + w && other.y + other.h <= y + h;
		}
	};

	struct Node_t
	{
		// Logical destination rect of the surface.
		Box_t rect;

		// Buffer-space source box and the buffer's size.
		float flSrcX = 0.0f, flSrcY = 0.0f, flSrcW = 0.0f, flSrcH = 0.0f;
		uint32_t uBufferWidth = 0;
		uint32_t uBufferHeight = 0;

		// Opaque region covers the whole surface.
		bool bOpaque = false;
		bool bHasContent = false;

		bool IsCropped() const
		{
			return flSrcX != 0.0f || flSrcY != 0.0f ||
				flSrcW != float( uBufferWidth ) || flSrcH != float( uBufferHeight );
		}
	};

	// Where the window's geometry box lands on the output.
	struct Fit_t
	{
		Box_t geometry;
		Vec2_t scale = { 1.0f, 1.0f };   // Output pixels per logical unit.
		Vec2_t offset = { 0.0f, 0.0f };  // Output position of the geometry origin.

		// Maps output pixels to root logical coordinates as ( out + offset ) * scale.
		Vec2_t InputScale() const { return { 1.0f / scale.x, 1.0f / scale.y }; }
		Vec2_t InputOffset() const
		{
			return { -offset.x + geometry.x * scale.x, -offset.y + geometry.y * scale.y };
		}
	};

	struct Layer_t
	{
		size_t uNode = 0;

		// Same convention as FrameInfo_t::Layer_t, texel = ( out + offset ) * scale.
		Vec2_t scale;
		Vec2_t offset;

		bool bCropped = false;
	};

	struct Result_t
	{
		// Bottom to top. The first layer is the base when there is one.
		std::vector<Layer_t> layers;
		std::optional<size_t> oBaseNode;

		// The base alone, unscaled and uncropped over the geometry, maps like a plain window.
		bool bPlainBase = false;
	};

	inline Layer_t MapNode( const std::vector<Node_t> &nodes, size_t uNode, const Fit_t &fit )
	{
		const Node_t &node = nodes[ uNode ];
		const float kx = node.flSrcW / float( node.rect.w );
		const float ky = node.flSrcH / float( node.rect.h );

		Layer_t layer;
		layer.uNode = uNode;
		layer.scale = { kx / fit.scale.x, ky / fit.scale.y };
		layer.offset =
		{
			-fit.offset.x + ( fit.geometry.x - node.rect.x ) * fit.scale.x + node.flSrcX / layer.scale.x,
			-fit.offset.y + ( fit.geometry.y - node.rect.y ) * fit.scale.y + node.flSrcY / layer.scale.y,
		};
		layer.bCropped = node.IsCropped();
		return layer;
	}

	// Nodes are bottom to top. Culls nodes hidden under a single opaque node above,
	// then keeps the base and the topmost nodes that fit in uMaxLayers.
	inline Result_t Resolve( const std::vector<Node_t> &nodes, const Fit_t &fit, size_t uMaxLayers )
	{
		Result_t result;

		std::vector<size_t> visible;
		for ( size_t i = 0; i < nodes.size(); i++ )
		{
			const Node_t &node = nodes[ i ];
			if ( !node.bHasContent || node.rect.w <= 0 || node.rect.h <= 0 || node.flSrcW <= 0.0f || node.flSrcH <= 0.0f )
				continue;

			bool bOccluded = false;
			for ( size_t j = i + 1; j < nodes.size() && !bOccluded; j++ )
			{
				const Node_t &above = nodes[ j ];
				bOccluded = above.bHasContent && above.bOpaque && above.rect.Contains( node.rect );
			}

			if ( !bOccluded )
				visible.push_back( i );
		}

		if ( visible.empty() || !uMaxLayers )
			return result;

		result.oBaseNode = visible.front();

		const size_t uTopCount = std::min( visible.size() - 1, uMaxLayers - 1 );
		result.layers.push_back( MapNode( nodes, visible.front(), fit ) );
		for ( size_t i = visible.size() - uTopCount; i < visible.size(); i++ )
			result.layers.push_back( MapNode( nodes, visible[ i ], fit ) );

		const Node_t &base = nodes[ visible.front() ];
		result.bPlainBase = visible.size() == 1 && !base.IsCropped() &&
			base.rect.x == fit.geometry.x && base.rect.y == fit.geometry.y &&
			base.rect.w == fit.geometry.w && base.rect.h == fit.geometry.h &&
			uint32_t( base.rect.w ) == base.uBufferWidth && uint32_t( base.rect.h ) == base.uBufferHeight;

		return result;
	}
}
