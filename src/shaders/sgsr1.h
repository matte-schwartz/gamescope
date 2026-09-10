/*
Copyright (c) 2023, 2025, Qualcomm Innovation Center, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

SPDX-License-Identifier: BSD-3-Clause
*/

// Adapted from Qualcomm SGSR1, d926f074bcb9d714e179f1ce0fcb9ee2eeb5074e.
// sgsr/v1/include/glsl/sgsr1_shader_mobile.frag (original GLSL variant).

mediump float sgsrFastLanczos2(mediump float x)
{
	mediump float wA = x-4.0;
	mediump float wB = x*wA-wA;
	wA *= wA;
	return wB*wA;
}
mediump vec2 sgsrWeight(mediump float dx, mediump float dy, mediump float c, mediump float std)
{
	mediump float x = ((dx*dx)+(dy* dy))* 0.55 + clamp(abs(c)*std, 0.0, 1.0);
	mediump float w = sgsrFastLanczos2(x);
	return vec2(w, w * c);
}

// strength scales the edge gain and the delta cap, so 0 leaves the bilinear sample.
vec4 sampleSgsr1(sampler2D tex, vec2 uv, mediump float strength)
{
	const int mode = 1; // Green serves as the brightness proxy in RGBA mode.
	const float edgeThreshold = 8.0 / 255.0;
	mediump vec4 color = textureLod(tex, uv, 0.0);

	vec2 size = vec2(textureSize(tex, 0));
	vec4 viewport = vec4(1.0 / size, size);
	{
		highp vec2 imgCoord = ((uv*viewport.zw)+vec2(-0.5,0.5));
		highp vec2 imgCoordPixel = floor(imgCoord);
		highp vec2 coord = (imgCoordPixel*viewport.xy);
		mediump vec2 pl = (imgCoord+(-imgCoordPixel));
		mediump vec4 left = textureGather(tex,coord, mode);

		mediump float edgeVote = abs(left.z - left.y) + abs(color[mode] - left.y)  + abs(color[mode] - left.z) ;
		if(edgeVote > edgeThreshold)
		{
			coord.x += viewport.x;

			mediump vec4 right = textureGather(tex,coord + vec2(viewport.x, 0.0), mode);
			mediump vec4 upDown;
			upDown.xy = textureGather(tex,coord + vec2(0.0, -viewport.y),mode).wz;
			upDown.zw  = textureGather(tex,coord+ vec2(0.0, viewport.y), mode).yx;

			mediump float mean = (left.y+left.z+right.x+right.w)*0.25;
			left = left - vec4(mean);
			right = right - vec4(mean);
			upDown = upDown - vec4(mean);
			mediump float center = color[mode] - mean;

			mediump float sum = (((((abs(left.x)+abs(left.y))+abs(left.z))+abs(left.w))+(((abs(right.x)+abs(right.y))+abs(right.z))+abs(right.w)))+(((abs(upDown.x)+abs(upDown.y))+abs(upDown.z))+abs(upDown.w)));
			mediump float std = 2.181818/sum;

			mediump vec2 aWY = sgsrWeight(pl.x, pl.y+1.0, upDown.x,std);
			aWY += sgsrWeight(pl.x-1.0, pl.y+1.0, upDown.y,std);
			aWY += sgsrWeight(pl.x-1.0, pl.y-2.0, upDown.z,std);
			aWY += sgsrWeight(pl.x, pl.y-2.0, upDown.w,std);
			aWY += sgsrWeight(pl.x+1.0, pl.y-1.0, left.x,std);
			aWY += sgsrWeight(pl.x, pl.y-1.0, left.y,std);
			aWY += sgsrWeight(pl.x, pl.y, left.z,std);
			aWY += sgsrWeight(pl.x+1.0, pl.y, left.w,std);
			aWY += sgsrWeight(pl.x-1.0, pl.y-1.0, right.x,std);
			aWY += sgsrWeight(pl.x-2.0, pl.y-1.0, right.y,std);
			aWY += sgsrWeight(pl.x-2.0, pl.y, right.z,std);
			aWY += sgsrWeight(pl.x-1.0, pl.y, right.w,std);

			mediump float finalY = aWY.y/aWY.x;

			mediump float maxY = max(max(left.y,left.z),max(right.x,right.w));
			mediump float minY = min(min(left.y,left.z),min(right.x,right.w));
			finalY = clamp((1.0 + strength)*finalY, minY, maxY);

			mediump float deltaY = finalY - center;

			//smooth high contrast input
			mediump float deltaCap = strength * 23.0 / 255.0;
			deltaY = clamp(deltaY, -deltaCap, deltaCap);

			color.x = clamp((color.x+deltaY),0.0,1.0);
			color.y = clamp((color.y+deltaY),0.0,1.0);
			color.z = clamp((color.z+deltaY),0.0,1.0);
		}
	}

	return color;
}
