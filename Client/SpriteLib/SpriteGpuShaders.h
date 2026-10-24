#pragma once

namespace SpriteGpuShaders {
inline constexpr const char* Fragment =
#ifdef __EMSCRIPTEN__
R"glsl(#version 300 es
precision highp float;
precision highp int;
#define varying in
// SDL GLES uploads ARGB8888 as RGBA bytes and swizzles it in its shaders.
// Share that storage convention when accessing SDL's textures directly.
#define texture2D(s, uv) texture(s, uv).bgra
#define gl_FragColor fragmentColor.bgra
out vec4 fragmentColor;
)glsl"
#else
"#version 120\n"
#endif
R"glsl(
uniform sampler2D source;
uniform sampler2D background;
uniform sampler2D palette;
uniform vec2 targetSize;
uniform int effect;
uniform float value;
uniform vec3 gradation[94];
varying vec2 uv;
varying float light;
void main() {
	vec4 pixel = texture2D(source, uv);
	if (pixel.a == 0.0) discard;
	// Convert expanded SDL pixels back to their original integer channels.
	vec3 bits = vec3(32.0, 64.0, 32.0);
	vec3 c = floor(floor(pixel.rgb * 255.0 + 0.5) / vec3(8.0, 4.0, 8.0));
	if (effect == 1) c = clamp(floor(c * value / 32.0), vec3(0.0), bits - 1.0);
	else if (effect == 2) {
		if (value == 0.0) c.gb = vec2(0.0);
		else if (value == 1.0) c.rb = vec2(0.0);
		else if (value == 2.0) c.rg = vec2(0.0);
	} else if (effect == 3) c = floor(c / exp2(clamp(value, 0.0, 6.0)));
	else if (effect == 4) {
		float gray = floor((c.r + floor(c.g / 2.0) + c.b) / 3.0);
		c = vec3(gray, gray * 2.0 + floor(gray / 16.0), gray);
	} else if (effect == 5) {
		int brightness = int(c.r + floor(c.g / 2.0) + c.b);
		c = gradation[brightness];
	} else if (effect == 9) {
		c = clamp(floor(c * floor(light + 0.5) / 32.0), vec3(0.0), bits - 1.0);
	} else if (effect == 10) {
		// value = alpha2 * 65536 + an RGB565 colour; alpha2 is the share kept.
		float alpha2 = floor(value / 65536.0);
		float color = value - alpha2 * 65536.0;
		vec3 s = vec3(floor(color / 2048.0), mod(floor(color / 32.0), 64.0), mod(color, 32.0));
		c = floor(s * (31.0 - alpha2) / 32.0) + floor(c * alpha2 / 32.0);
	} else if (effect >= 6) {
		float index = floor(pixel.r * 255.0 + 0.5);
		vec3 color = texture2D(palette, vec2((index + 0.5) / 256.0, 0.5)).rgb;
		c = floor(floor(color * 255.0 + 0.5) / vec3(8.0, 4.0, 8.0));
		if (effect >= 7) {
			vec3 dest = texture2D(background, gl_FragCoord.xy / targetSize).rgb;
			dest = floor(floor(dest * 255.0 + 0.5) / vec3(8.0, 4.0, 8.0));
			if (effect == 7) c = max(dest, c) + floor(min(dest, c) * (bits - max(dest, c)) / bits);
			else c = dest + floor((c - dest) * floor(pixel.g * 255.0 + 0.5) / 32.0);
		}
	}
	// Expand bits exactly as SDL does so a later RGB565 readback is lossless.
	vec3 rgb = c * vec3(8.0, 4.0, 8.0) + floor(c / vec3(4.0, 16.0, 4.0));
	gl_FragColor = vec4(rgb / 255.0, 1.0);
}
)glsl";
}
