/*
Simple Gradient Stripes
Copyright (C) 2026 AllegroMoltoV

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>

#include <algorithm>
#include <cmath>
#include <new>

namespace {

constexpr const char *kSourceId = "simple_gradient_stripes_source";

constexpr const char *kAngle = "angle";
constexpr const char *kSpacing = "spacing";
constexpr const char *kSpeed = "speed";
constexpr const char *kStripeColor = "stripe_color";
constexpr const char *kBackgroundColor = "background_color";
constexpr const char *kSoftness = "softness";
constexpr const char *kUseCanvasSize = "use_canvas_size";
constexpr const char *kWidth = "width";
constexpr const char *kHeight = "height";

constexpr int kMinSpacing = 4;
constexpr int kMaxSpacing = 8192;
constexpr int kMinSize = 1;
constexpr int kMaxSize = 8192;
// 流れの向きは角度で決めるため、速さは 0 以上だけにする
constexpr double kMinSpeed = 0.0;
constexpr double kMaxSpeed = 2000.0;
// ぼかし (%)。100% を超えると、しまの濃淡が薄れていく
constexpr int kMinSoftness = 0;
constexpr int kMaxSoftness = 200;

int get_int_clamped(obs_data_t *settings, const char *name, int min, int max)
{
	return static_cast<int>(std::clamp<long long>(obs_data_get_int(settings, name), min, max));
}

constexpr double kPi = 3.14159265358979323846;

class GradientStripesSource final {
public:
	explicit GradientStripesSource(obs_data_t *settings)
	{
		obs_enter_graphics();
		char *path = obs_module_file("gradient-stripes.effect");
		if (path) {
			effect_ = gs_effect_create_from_file(path, nullptr);
			bfree(path);
		}
		if (effect_) {
			param_size_ = gs_effect_get_param_by_name(effect_, "size");
			param_normal_ = gs_effect_get_param_by_name(effect_, "stripe_normal");
			param_spacing_ = gs_effect_get_param_by_name(effect_, "spacing");
			param_offset_ = gs_effect_get_param_by_name(effect_, "flow_offset");
			param_kernel_width_ = gs_effect_get_param_by_name(effect_, "kernel_width");
			param_stripe_color_ = gs_effect_get_param_by_name(effect_, "stripe_color");
			param_background_color_ = gs_effect_get_param_by_name(effect_, "background_color");
		} else {
			obs_log(LOG_WARNING, "[gradient_stripes] Failed to load effect: gradient-stripes.effect");
		}
		obs_leave_graphics();

		Update(settings);
	}

	~GradientStripesSource()
	{
		obs_enter_graphics();
		gs_effect_destroy(effect_);
		obs_leave_graphics();
	}

	GradientStripesSource(const GradientStripesSource &) = delete;
	GradientStripesSource &operator=(const GradientStripesSource &) = delete;

	void Update(obs_data_t *settings)
	{
		const double angle = obs_data_get_double(settings, kAngle) * kPi / 180.0;
		// しまの線の向きは (cos, sin)。その法線へ流れる。y は上向き
		vec2_set(&normal_, static_cast<float>(-std::sin(angle)), static_cast<float>(std::cos(angle)));
		spacing_ = get_int_clamped(settings, kSpacing, kMinSpacing, kMaxSpacing);
		speed_ = std::clamp(obs_data_get_double(settings, kSpeed), kMinSpeed, kMaxSpeed);
		softness_ =
			static_cast<float>(get_int_clamped(settings, kSoftness, kMinSoftness, kMaxSoftness)) / 100.0f;
		use_canvas_size_ = obs_data_get_bool(settings, kUseCanvasSize);
		width_ = static_cast<uint32_t>(get_int_clamped(settings, kWidth, kMinSize, kMaxSize));
		height_ = static_cast<uint32_t>(get_int_clamped(settings, kHeight, kMinSize, kMaxSize));
		// 色は常に線形の値で混ぜる。不透明度で経路を切り替えると、グラデーションの見え方が不連続に変わるため
		vec4_from_rgba_srgb(&stripe_color_, static_cast<uint32_t>(obs_data_get_int(settings, kStripeColor)));
		vec4_from_rgba_srgb(&background_color_,
				    static_cast<uint32_t>(obs_data_get_int(settings, kBackgroundColor)));
	}

	uint32_t Width() const
	{
		if (!use_canvas_size_) {
			return width_;
		}
		obs_video_info ovi;
		return obs_get_video_info(&ovi) ? ovi.base_width : 1920;
	}

	uint32_t Height() const
	{
		if (!use_canvas_size_) {
			return height_;
		}
		obs_video_info ovi;
		return obs_get_video_info(&ovi) ? ovi.base_height : 1080;
	}

	void Tick(float seconds)
	{
		// 位置は周期に対する割合で持つ。間隔を変えても模様の位相が飛ばないようにするため
		offset_ += speed_ * static_cast<double>(seconds) / static_cast<double>(spacing_);
		offset_ -= std::floor(offset_);
	}

	void Render()
	{
		if (!effect_) {
			return;
		}

		const uint32_t width_px = Width();
		const uint32_t height_px = Height();
		const float spacing = static_cast<float>(spacing_);

		vec2 size;
		vec2_set(&size, static_cast<float>(width_px), static_cast<float>(height_px));
		// 三角形のカーネルの半幅を、周期に対する割合で求める。ぼかし 100% で周期の 1/4 (しまの幅の半分) になり、
		// 縁から縁までがなめらかにつながる。それを超えると濃淡が薄れ、200% で半分になる。
		// 下限の 1 画素は、ぼかし 0 でも縁がぎざぎざにならないようにするため
		const float kernel_width = std::max(0.25f * softness_, 1.0f / spacing);

		gs_effect_set_vec2(param_size_, &size);
		gs_effect_set_vec2(param_normal_, &normal_);
		gs_effect_set_float(param_spacing_, spacing);
		gs_effect_set_float(param_offset_, static_cast<float>(offset_));
		gs_effect_set_float(param_kernel_width_, kernel_width);
		gs_effect_set_vec4(param_stripe_color_, &stripe_color_);
		gs_effect_set_vec4(param_background_color_, &background_color_);

		// 線形の色で描き、フレームバッファの sRGB 変換を有効にする。合成の方法は呼び出し側の設定に従う
		const bool previous = gs_framebuffer_srgb_enabled();
		gs_enable_framebuffer_srgb(true);
		while (gs_effect_loop(effect_, "Draw")) {
			gs_draw_sprite(nullptr, 0, width_px, height_px);
		}
		gs_enable_framebuffer_srgb(previous);
	}

private:
	gs_effect_t *effect_ = nullptr;
	gs_eparam_t *param_size_ = nullptr;
	gs_eparam_t *param_normal_ = nullptr;
	gs_eparam_t *param_spacing_ = nullptr;
	gs_eparam_t *param_offset_ = nullptr;
	gs_eparam_t *param_kernel_width_ = nullptr;
	gs_eparam_t *param_stripe_color_ = nullptr;
	gs_eparam_t *param_background_color_ = nullptr;

	vec2 normal_ = {};
	int spacing_ = 120;
	double speed_ = 40.0;
	float softness_ = 0.5f;
	bool use_canvas_size_ = true;
	uint32_t width_ = 1920;
	uint32_t height_ = 1080;
	vec4 stripe_color_ = {};
	vec4 background_color_ = {};

	double offset_ = 0.0;
};

const char *source_get_name(void *)
{
	return obs_module_text("GradientStripes");
}

void *source_create(obs_data_t *settings, obs_source_t *)
{
	return new (std::nothrow) GradientStripesSource(settings);
}

void source_destroy(void *data)
{
	delete static_cast<GradientStripesSource *>(data);
}

void source_update(void *data, obs_data_t *settings)
{
	static_cast<GradientStripesSource *>(data)->Update(settings);
}

void source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, kAngle, 30.0);
	obs_data_set_default_int(settings, kSpacing, 120);
	obs_data_set_default_double(settings, kSpeed, 100.0);
	// 既定の 2 色はロゴと同じ水色 (#9fe7ff) と藍 (#3a3fd6)。OBS の色は 0xAABBGGRR で、どちらも不透明
	obs_data_set_default_int(settings, kStripeColor, 0xFFFFE79F);
	obs_data_set_default_int(settings, kBackgroundColor, 0xFFD63F3A);
	obs_data_set_default_int(settings, kSoftness, 50);
	obs_data_set_default_bool(settings, kUseCanvasSize, true);
	obs_data_set_default_int(settings, kWidth, 1920);
	obs_data_set_default_int(settings, kHeight, 1080);
}

bool use_canvas_size_modified(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const bool custom = !obs_data_get_bool(settings, kUseCanvasSize);
	obs_property_set_visible(obs_properties_get(props, kWidth), custom);
	obs_property_set_visible(obs_properties_get(props, kHeight), custom);
	return true;
}

obs_properties_t *source_get_properties(void *)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *p = obs_properties_add_float_slider(props, kAngle, obs_module_text("Angle"), 0.0, 360.0, 1.0);
	// 度の記号を UTF-8 のバイト列で書く。コンパイラの実行文字セットに依存しないようにするため
	obs_property_float_set_suffix(p, "\xC2\xB0");
	p = obs_properties_add_int_slider(props, kSpacing, obs_module_text("Spacing"), kMinSpacing, kMaxSpacing, 1);
	obs_property_int_set_suffix(p, " px");
	p = obs_properties_add_float_slider(props, kSpeed, obs_module_text("Speed"), kMinSpeed, kMaxSpeed, 1.0);
	obs_property_float_set_suffix(p, " px/s");
	obs_properties_add_color_alpha(props, kStripeColor, obs_module_text("StripeColor"));
	obs_properties_add_color_alpha(props, kBackgroundColor, obs_module_text("BackgroundColor"));
	p = obs_properties_add_int_slider(props, kSoftness, obs_module_text("Softness"), kMinSoftness, kMaxSoftness, 1);
	obs_property_int_set_suffix(p, " %");

	p = obs_properties_add_bool(props, kUseCanvasSize, obs_module_text("UseCanvasSize"));
	obs_property_set_modified_callback(p, use_canvas_size_modified);
	obs_properties_add_int(props, kWidth, obs_module_text("Width"), kMinSize, kMaxSize, 1);
	obs_properties_add_int(props, kHeight, obs_module_text("Height"), kMinSize, kMaxSize, 1);

	return props;
}

void source_video_tick(void *data, float seconds)
{
	static_cast<GradientStripesSource *>(data)->Tick(seconds);
}

uint32_t source_get_width(void *data)
{
	return static_cast<GradientStripesSource *>(data)->Width();
}

uint32_t source_get_height(void *data)
{
	return static_cast<GradientStripesSource *>(data)->Height();
}

void source_video_render(void *data, gs_effect_t *)
{
	static_cast<GradientStripesSource *>(data)->Render();
}

} // namespace

extern "C" void register_gradient_stripes_source(void)
{
	obs_source_info info = {};
	info.id = kSourceId;
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	info.get_name = source_get_name;
	info.create = source_create;
	info.destroy = source_destroy;
	info.update = source_update;
	info.get_defaults = source_get_defaults;
	info.get_properties = source_get_properties;
	info.video_tick = source_video_tick;
	info.get_width = source_get_width;
	info.get_height = source_get_height;
	info.video_render = source_video_render;
	info.icon_type = OBS_ICON_TYPE_COLOR;
	obs_register_source(&info);
}
