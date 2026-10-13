#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Utils/SurfaceTree.h"

using namespace gamescope::SurfaceTree;
using Catch::Matchers::WithinAbs;

namespace
{
	Node_t MakeNode( Box_t rect, bool bOpaque = false, uint32_t uBufferWidth = 0, uint32_t uBufferHeight = 0 )
	{
		Node_t node;
		node.rect = rect;
		node.uBufferWidth = uBufferWidth ? uBufferWidth : uint32_t( rect.w );
		node.uBufferHeight = uBufferHeight ? uBufferHeight : uint32_t( rect.h );
		node.flSrcW = float( node.uBufferWidth );
		node.flSrcH = float( node.uBufferHeight );
		node.bOpaque = bOpaque;
		node.bHasContent = true;
		return node;
	}

	Fit_t MakeFit( Box_t geometry, float flScale = 1.0f, Vec2_t offset = {} )
	{
		return Fit_t{ geometry, { flScale, flScale }, offset };
	}

	// Output pixel that samples a node's texel, by inverting texel = ( out + offset ) * scale.
	Vec2_t OutputOf( const Layer_t &layer, Vec2_t texel )
	{
		return { texel.x / layer.scale.x - layer.offset.x, texel.y / layer.scale.y - layer.offset.y };
	}
}

TEST_CASE("An opaque child covering the root becomes the base", "[surface_tree]") {
	std::vector<Node_t> nodes{ MakeNode( { 0, 0, 1280, 720 } ), MakeNode( { 0, 0, 1280, 720 }, true ) };
	Result_t result = Resolve( nodes, MakeFit( { 0, 0, 1280, 720 } ), 8 );
	REQUIRE( result.oBaseNode == 1 );
	REQUIRE( result.layers.size() == 1 );
	REQUIRE( result.bPlainBase );
}

TEST_CASE("A translucent child keeps the root under it", "[surface_tree]") {
	std::vector<Node_t> nodes{ MakeNode( { 0, 0, 1280, 720 } ), MakeNode( { 0, 0, 1280, 720 } ) };
	Result_t result = Resolve( nodes, MakeFit( { 0, 0, 1280, 720 } ), 8 );
	REQUIRE( result.oBaseNode == 0 );
	REQUIRE( result.layers.size() == 2 );
	REQUIRE_FALSE( result.bPlainBase );
}

TEST_CASE("A partially covering opaque child culls nothing", "[surface_tree]") {
	std::vector<Node_t> nodes{ MakeNode( { 0, 0, 1280, 720 } ), MakeNode( { 0, 30, 1280, 720 }, true ) };
	Result_t result = Resolve( nodes, MakeFit( { 0, 0, 1280, 750 } ), 8 );
	REQUIRE( result.oBaseNode == 0 );
	REQUIRE( result.layers.size() == 2 );
}

TEST_CASE("Nodes without content are skipped and do not occlude", "[surface_tree]") {
	std::vector<Node_t> nodes{ MakeNode( { 0, 0, 100, 100 } ), MakeNode( { 0, 0, 100, 100 }, true ) };
	nodes[ 1 ].bHasContent = false;
	Result_t result = Resolve( nodes, MakeFit( { 0, 0, 100, 100 } ), 8 );
	REQUIRE( result.oBaseNode == 0 );
	REQUIRE( result.layers.size() == 1 );

	nodes[ 0 ].bHasContent = false;
	REQUIRE_FALSE( Resolve( nodes, MakeFit( { 0, 0, 100, 100 } ), 8 ).oBaseNode );
}

TEST_CASE("Over budget keeps the base and the topmost nodes", "[surface_tree]") {
	std::vector<Node_t> nodes;
	for ( int i = 0; i < 5; i++ )
		nodes.push_back( MakeNode( { i * 10, 0, 10, 10 } ) );
	Result_t result = Resolve( nodes, MakeFit( { 0, 0, 50, 10 } ), 3 );
	REQUIRE( result.layers.size() == 3 );
	REQUIRE( result.layers[ 0 ].uNode == 0 );
	REQUIRE( result.layers[ 1 ].uNode == 3 );
	REQUIRE( result.layers[ 2 ].uNode == 4 );
}

TEST_CASE("A viewport destination scales the buffer onto its logical rect", "[surface_tree]") {
	// A 640x360 swapchain stretched over a 1280x720 client rect at (0, 40), shown at 1.5x.
	std::vector<Node_t> nodes{ MakeNode( { 0, 40, 1280, 720 }, true, 640, 360 ) };
	Fit_t fit = MakeFit( { 0, 0, 1280, 760 }, 1.5f, { 10.0f, 20.0f } );
	Result_t result = Resolve( nodes, fit, 8 );
	REQUIRE( result.layers.size() == 1 );
	REQUIRE_FALSE( result.bPlainBase );

	const Layer_t &layer = result.layers[ 0 ];
	Vec2_t origin = OutputOf( layer, { 0.0f, 0.0f } );
	REQUIRE_THAT( origin.x, WithinAbs( 10.0f, 1e-3 ) );
	REQUIRE_THAT( origin.y, WithinAbs( 20.0f + 40.0f * 1.5f, 1e-3 ) );
	Vec2_t corner = OutputOf( layer, { 640.0f, 360.0f } );
	REQUIRE_THAT( corner.x, WithinAbs( 10.0f + 1280.0f * 1.5f, 1e-3 ) );
	REQUIRE_THAT( corner.y, WithinAbs( 20.0f + 760.0f * 1.5f, 1e-3 ) );
}

TEST_CASE("A source crop maps the cropped box onto the logical rect", "[surface_tree]") {
	// A 2x scale buffer of 2000x1000 cropped to its top-left 1600x900.
	Node_t node = MakeNode( { 0, 0, 800, 450 }, false, 2000, 1000 );
	node.flSrcX = 100.0f;
	node.flSrcY = 50.0f;
	node.flSrcW = 1600.0f;
	node.flSrcH = 900.0f;
	std::vector<Node_t> nodes{ node };
	Result_t result = Resolve( nodes, MakeFit( { 0, 0, 800, 450 } ), 8 );
	REQUIRE( result.layers[ 0 ].bCropped );
	REQUIRE_FALSE( result.bPlainBase );

	Vec2_t origin = OutputOf( result.layers[ 0 ], { 100.0f, 50.0f } );
	REQUIRE_THAT( origin.x, WithinAbs( 0.0f, 1e-3 ) );
	REQUIRE_THAT( origin.y, WithinAbs( 0.0f, 1e-3 ) );
	Vec2_t corner = OutputOf( result.layers[ 0 ], { 1700.0f, 950.0f } );
	REQUIRE_THAT( corner.x, WithinAbs( 800.0f, 1e-3 ) );
	REQUIRE_THAT( corner.y, WithinAbs( 450.0f, 1e-3 ) );
}

TEST_CASE("A geometry offset shifts every node", "[surface_tree]") {
	// CSD shadow of 20 around the window, geometry starts inside the root surface.
	std::vector<Node_t> nodes{ MakeNode( { 0, 0, 840, 640 } ), MakeNode( { 300, 200, 100, 50 } ) };
	Result_t result = Resolve( nodes, MakeFit( { 20, 20, 800, 600 } ), 8 );
	Vec2_t root = OutputOf( result.layers[ 0 ], { 20.0f, 20.0f } );
	REQUIRE_THAT( root.x, WithinAbs( 0.0f, 1e-3 ) );
	REQUIRE_THAT( root.y, WithinAbs( 0.0f, 1e-3 ) );
	Vec2_t child = OutputOf( result.layers[ 1 ], { 0.0f, 0.0f } );
	REQUIRE_THAT( child.x, WithinAbs( 280.0f, 1e-3 ) );
	REQUIRE_THAT( child.y, WithinAbs( 180.0f, 1e-3 ) );
}

TEST_CASE("Input mapping returns root logical coordinates", "[surface_tree]") {
	Fit_t fit = MakeFit( { 20, 20, 800, 600 }, 2.0f, { 100.0f, 50.0f } );
	Vec2_t scale = fit.InputScale();
	Vec2_t offset = fit.InputOffset();
	// The geometry origin lands at output (100, 50).
	REQUIRE_THAT( ( 100.0f + offset.x ) * scale.x, WithinAbs( 20.0f, 1e-3 ) );
	REQUIRE_THAT( ( 50.0f + offset.y ) * scale.y, WithinAbs( 20.0f, 1e-3 ) );
	REQUIRE_THAT( ( 100.0f + 1600.0f + offset.x ) * scale.x, WithinAbs( 820.0f, 1e-3 ) );
}
