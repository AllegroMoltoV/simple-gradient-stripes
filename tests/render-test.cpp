// Simple Gradient Stripes の描画テスト。
// libobs を画面なしで起動してソースを作り、出力の画素を読み戻して、しまの向き・周期・ぼかし・色・流れを確かめる。
// レンダラーごとの丸めの差を吸収するため、画像の完全一致ではなく許容差つきの性質で比べる。
//
// 使い方: render-test <plugin module> <plugin data folder> <graphics module> <libobs data folder> <scenario>
// 終了コード: すべての検査が通れば 0、検査に落ちれば 1、テストを始められなければ 2

#include "render-fixture.hpp"

#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <sstream>

namespace {

using namespace render_test;

constexpr const char *kSourceId = "simple_gradient_stripes_source";

constexpr uint32_t kCanvasWidth = 160;
constexpr uint32_t kCanvasHeight = 120;

// 白の不透明なしまを透明な地に描き、しまの割合を不透明度で読む。速さ 0 で模様を止める
using SettingsWriter = std::function<void(obs_data_t *)>;

obs_source_t *create_stripes(const SettingsWriter &write)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_double(settings, "angle", 0.0);
	obs_data_set_int(settings, "spacing", 32);
	obs_data_set_double(settings, "speed", 0.0);
	obs_data_set_int(settings, "stripe_color", 0xFFFFFFFF);
	obs_data_set_int(settings, "background_color", 0x00000000);
	obs_data_set_int(settings, "softness", 0);
	if (write) {
		write(settings);
	}
	obs_source_t *source = obs_source_create(kSourceId, "stripes", settings, nullptr);
	obs_data_release(settings);
	return source;
}

// 設定が video_tick で反映されるのを待ち、macOS の最初の読み戻しを捨ててから描く
bool render_settled(obs_source_t *source, Image &image)
{
	Image warmup;
	return wait_frames(2) && render(source, warmup) && render(source, image);
}

int alpha(const Image &image, uint32_t x, uint32_t y)
{
	return image.at(x, y)[3];
}

std::string at(uint32_t x, uint32_t y)
{
	std::ostringstream s;
	s << "(" << x << ", " << y << ")";
	return s.str();
}

// 列 x の不透明度に、ほぼ透明としまの色の両方が現れることを確かめる
void expect_column_varies(Checker &c, const Image &image, uint32_t x)
{
	int low = 0;
	int high = 0;
	for (uint32_t y = 0; y < image.height; ++y) {
		const int a = alpha(image, x, y);
		low += a < 20 ? 1 : 0;
		high += a > 235 ? 1 : 0;
	}
	c.expect(low > 0 && high > 0, "column " + std::to_string(x) + " does not contain both stripe and background");
}

// 角度 0° では線が水平で、行の中の画素が一定になり、縦にしまが並ぶ
void scenario_horizontal(Checker &c)
{
	obs_source_t *source = create_stripes(nullptr);
	Image image;
	if (!source || !render_settled(source, image)) {
		c.expect(false, "failed to render the source");
	} else {
		save_image("horizontal", image);
		for (uint32_t y = 0; y < image.height; ++y) {
			for (uint32_t x = 1; x < image.width; ++x) {
				c.expect(std::abs(alpha(image, x, y) - alpha(image, 0, y)) <= 3,
					 "row is not uniform at " + at(x, y));
			}
		}
		expect_column_varies(c, image, image.width / 2);
	}
	obs_source_release(source);
}

// 角度 90° では線が垂直で、列の中の画素が一定になり、横にしまが並ぶ
void scenario_vertical(Checker &c)
{
	obs_source_t *source = create_stripes([](obs_data_t *s) { obs_data_set_double(s, "angle", 90.0); });
	Image image;
	if (!source || !render_settled(source, image)) {
		c.expect(false, "failed to render the source");
	} else {
		save_image("vertical", image);
		for (uint32_t x = 0; x < image.width; ++x) {
			for (uint32_t y = 1; y < image.height; ++y) {
				c.expect(std::abs(alpha(image, x, y) - alpha(image, x, 0)) <= 3,
					 "column is not uniform at " + at(x, y));
			}
		}
		int low = 0;
		int high = 0;
		for (uint32_t x = 0; x < image.width; ++x) {
			const int a = alpha(image, x, image.height / 2);
			low += a < 20 ? 1 : 0;
			high += a > 235 ? 1 : 0;
		}
		c.expect(low > 0 && high > 0, "the middle row does not contain both stripe and background");
	}
	obs_source_release(source);
}

// 角度 45° では、線が画面の右上がりに伸びる。角度は画面の上で反時計回りに回る
void scenario_rotation(Checker &c)
{
	obs_source_t *source = create_stripes([](obs_data_t *s) { obs_data_set_double(s, "angle", 45.0); });
	Image image;
	if (!source || !render_settled(source, image)) {
		c.expect(false, "failed to render the source");
	} else {
		save_image("rotation", image);
		// 右上がりの対角線 (x + k, y - k) の上では一定
		const uint32_t y0 = image.height - 1;
		for (uint32_t x0 = 0; x0 < image.width; x0 += 7) {
			for (uint32_t k = 1; x0 + k < image.width && k <= y0; ++k) {
				c.expect(std::abs(alpha(image, x0 + k, y0 - k) - alpha(image, x0, y0)) <= 3,
					 "not uniform along the rising diagonal from " + at(x0, y0));
			}
		}
		// 右下がりの対角線 (k, k) の上では、しまと地が入れ替わる
		int low = 0;
		int high = 0;
		for (uint32_t k = 0; k < image.width && k < image.height; ++k) {
			const int a = alpha(image, k, k);
			low += a < 20 ? 1 : 0;
			high += a > 235 ? 1 : 0;
		}
		c.expect(low > 0 && high > 0, "the falling diagonal does not cross the stripes");
	}
	obs_source_release(source);
}

// 間隔の値が 1 周期の長さになる。半周期ずれた画素では、しまと地が入れ替わる
void scenario_period(Checker &c)
{
	constexpr uint32_t kSpacing = 40;
	obs_source_t *source = create_stripes([](obs_data_t *s) { obs_data_set_int(s, "spacing", kSpacing); });
	Image image;
	if (!source || !render_settled(source, image)) {
		c.expect(false, "failed to render the source");
	} else {
		save_image("period", image);
		const uint32_t x = image.width / 2;
		int inverted = 0;
		int checked = 0;
		for (uint32_t y = 0; y + kSpacing < image.height; ++y) {
			c.expect(std::abs(alpha(image, x, y + kSpacing) - alpha(image, x, y)) <= 3,
				 "pixels one period apart differ at " + at(x, y));
			const int a = alpha(image, x, y);
			const int half = alpha(image, x, y + kSpacing / 2);
			if (a < 20 || a > 235) {
				++checked;
				inverted += ((a < 20) != (half < 20)) ? 1 : 0;
			}
		}
		c.expect(checked > 0 && inverted * 10 >= checked * 9,
			 "stripe and background do not swap at half a period");
	}
	obs_source_release(source);
}

// 列の中で、しまと地の中間の不透明度を持つ画素の割合
double intermediate_ratio(const Image &image, uint32_t x)
{
	int intermediate = 0;
	for (uint32_t y = 0; y < image.height; ++y) {
		const int a = alpha(image, x, y);
		intermediate += (a > 20 && a < 235) ? 1 : 0;
	}
	return static_cast<double>(intermediate) / image.height;
}

// ぼかし 0 では縁だけが中間の色になり、ぼかし 100 では中間の色が広がる
void scenario_softness(Checker &c)
{
	obs_source_t *sharp = create_stripes(nullptr);
	obs_source_t *soft = create_stripes([](obs_data_t *s) { obs_data_set_int(s, "softness", 100); });
	Image sharp_image;
	Image soft_image;
	if (!sharp || !soft || !render_settled(sharp, sharp_image) || !render(soft, soft_image)) {
		c.expect(false, "failed to render the sources");
	} else {
		save_image("softness-0", sharp_image);
		save_image("softness-100", soft_image);
		const uint32_t x = sharp_image.width / 2;
		// 間隔 32 px で縁は 1 周期に 2 つ。縁 1 つあたり 2 画素までを中間の色として許す
		const double sharp_ratio = intermediate_ratio(sharp_image, x);
		const double soft_ratio = intermediate_ratio(soft_image, x);
		c.expect(sharp_ratio <= 4.0 / 32.0 + 0.01,
			 "softness 0 has too many intermediate pixels: " + std::to_string(sharp_ratio));
		c.expect(soft_ratio >= 0.5,
			 "softness 100 has too few intermediate pixels: " + std::to_string(soft_ratio));
	}
	obs_source_release(soft);
	obs_source_release(sharp);
}

// ぼかし 200% では三角形のカーネルがしまの幅いっぱいに広がり、濃淡が半分になる。
// しまの中央の割合は 0.75、地の中央は 0.25 で、不透明度はおよそ 191 と 64 になる
void scenario_softness_over_100(Checker &c)
{
	obs_source_t *source = create_stripes([](obs_data_t *s) { obs_data_set_int(s, "softness", 200); });
	Image image;
	if (!source || !render_settled(source, image)) {
		c.expect(false, "failed to render the source");
	} else {
		save_image("softness-200", image);
		const uint32_t x = image.width / 2;
		int max_alpha = 0;
		int min_alpha = 255;
		for (uint32_t y = 0; y < image.height; ++y) {
			max_alpha = std::max(max_alpha, alpha(image, x, y));
			min_alpha = std::min(min_alpha, alpha(image, x, y));
		}
		c.expect(std::abs(max_alpha - 191) <= 6,
			 "softness 200 stripe center alpha: " + std::to_string(max_alpha));
		c.expect(std::abs(min_alpha - 64) <= 6,
			 "softness 200 background center alpha: " + std::to_string(min_alpha));
	}
	obs_source_release(source);
}

// 何も設定しないとき、出力は不透明で、しまと地にそれぞれ既定の色が出る。
// 既定のしまの色は水色 (159, 231, 255)、地の色は藍 (58, 63, 214)
void scenario_defaults(Checker &c)
{
	obs_source_t *source = obs_source_create(kSourceId, "defaults", nullptr, nullptr);
	Image image;
	if (!source || !render_settled(source, image)) {
		c.expect(false, "failed to render the source");
	} else {
		save_image("defaults", image);
		const auto near = [](const uint8_t *p, int r, int g, int b) {
			return std::abs(p[0] - r) <= 4 && std::abs(p[1] - g) <= 4 && std::abs(p[2] - b) <= 4;
		};
		bool all_opaque = true;
		bool has_stripe = false;
		bool has_background = false;
		for (uint32_t y = 0; y < image.height; ++y) {
			for (uint32_t x = 0; x < image.width; ++x) {
				const uint8_t *p = image.at(x, y);
				all_opaque = all_opaque && p[3] >= 254;
				has_stripe = has_stripe || near(p, 159, 231, 255);
				has_background = has_background || near(p, 58, 63, 214);
			}
		}
		c.expect(all_opaque, "the default output is not opaque");
		c.expect(has_stripe, "the default stripe color does not appear");
		c.expect(has_background, "the default background color does not appear");
	}
	obs_source_release(source);
}

// 地が透明のとき、しまの縁の半透明の画素の色はしまの色のままになる。黒くにじまない
void scenario_transparent_background(Checker &c)
{
	// しまの色は 0xAABBGGRR の形で、R 255、G 128、B 0、不透明
	obs_source_t *source = create_stripes([](obs_data_t *s) {
		obs_data_set_int(s, "stripe_color", 0xFF0080FF);
		obs_data_set_int(s, "softness", 100);
	});
	Image image;
	if (!source || !render_settled(source, image)) {
		c.expect(false, "failed to render the source");
	} else {
		save_image("transparent-background", image);
		const uint32_t x = image.width / 2;
		int min_alpha = 255;
		int translucent = 0;
		for (uint32_t y = 0; y < image.height; ++y) {
			const uint8_t *p = image.at(x, y);
			min_alpha = std::min<int>(min_alpha, p[3]);
			if (p[3] >= 30 && p[3] <= 225) {
				++translucent;
				c.expect(std::abs(p[0] - 255) <= 4 && std::abs(p[1] - 128) <= 4 &&
						 std::abs(p[2] - 0) <= 4,
					 "translucent pixel at " + at(x, y) +
						 " is not the stripe color: " + std::to_string(p[0]) + ", " +
						 std::to_string(p[1]) + ", " + std::to_string(p[2]));
			}
		}
		c.expect(min_alpha <= 5, "the background between stripes is not transparent");
		c.expect(translucent > 0, "no translucent pixels to check");
	}
	obs_source_release(source);
}

// 速さ 0 では、時間がたっても模様が動かない
void scenario_still(Checker &c)
{
	obs_source_t *source = create_stripes([](obs_data_t *s) { obs_data_set_int(s, "softness", 100); });
	Image before;
	Image after;
	if (!source || !render_settled(source, before) || !wait_frames(30) || !render(source, after)) {
		c.expect(false, "failed to render the source");
	} else {
		save_image("still", after);
		bool same = before.rgba.size() == after.rgba.size();
		for (size_t i = 0; same && i < before.rgba.size(); ++i) {
			same = std::abs(before.rgba[i] - after.rgba[i]) <= 1;
		}
		c.expect(same, "the pattern moved with speed 0");
	}
	obs_source_release(source);
}

// 列 x の不透明度を、行をずらして比べたときの差の平均
double profile_difference(const Image &a, const Image &b, uint32_t x, int shift)
{
	double sum = 0.0;
	int count = 0;
	for (int y = 0; y < static_cast<int>(a.height); ++y) {
		const int ys = y + shift;
		if (ys < 0 || ys >= static_cast<int>(a.height)) {
			continue;
		}
		sum += std::abs(alpha(b, x, static_cast<uint32_t>(y)) - alpha(a, x, static_cast<uint32_t>(ys)));
		++count;
	}
	return count > 0 ? sum / count : std::numeric_limits<double>::max();
}

// 正の速さでは、角度 0° の模様が画面の上へ流れる。動く量は速さと経過したフレーム数に見合う
void scenario_flow(Checker &c)
{
	constexpr int kSpacing = 120;
	constexpr double kSpeed = 40.0;
	obs_source_t *source = create_stripes([](obs_data_t *s) {
		obs_data_set_int(s, "spacing", kSpacing);
		obs_data_set_double(s, "speed", kSpeed);
		obs_data_set_int(s, "softness", 100);
	});
	Image before;
	Image after;
	uint64_t frames_before = 0;
	uint64_t frames_after = 0;
	bool ok = source && render_settled(source, before);
	if (ok) {
		frames_before = obs_get_total_frames();
		ok = wait_frames(30);
		frames_after = obs_get_total_frames();
		ok = ok && render(source, after);
	}
	if (!ok) {
		c.expect(false, "failed to render the source");
	} else {
		save_image("flow-before", before);
		save_image("flow-after", after);
		// 後の画像の行 y に、前の画像の行 y + shift の内容が来る。上へ流れれば shift は正になる
		const uint32_t x = before.width / 2;
		int best_shift = 0;
		double best = std::numeric_limits<double>::max();
		for (int shift = -kSpacing / 2 + 1; shift < kSpacing / 2; ++shift) {
			const double d = profile_difference(before, after, x, shift);
			if (d < best) {
				best = d;
				best_shift = shift;
			}
		}
		// 描画とフレームの数え方の間に 2 フレームほどの差が出るため、その分の幅を持たせる
		const double frames = static_cast<double>(frames_after - frames_before);
		const double expected_min = kSpeed * (frames - 2.0) / 60.0;
		const double expected_max = kSpeed * (frames + 4.0) / 60.0;
		std::cout << "flow shift " << best_shift << " px over " << frames << " frames (difference " << best
			  << ")\n";
		c.expect(best_shift > 0, "the pattern did not move up: shift " + std::to_string(best_shift));
		c.expect(best_shift >= expected_min - 2.0 && best_shift <= expected_max + 2.0,
			 "the pattern moved " + std::to_string(best_shift) + " px, expected " +
				 std::to_string(expected_min) + " to " + std::to_string(expected_max));
		c.expect(best <= 4.0, "the moved pattern does not match: " + std::to_string(best));
	}
	obs_source_release(source);
}

// キャンバスに合わせる設定がオンならキャンバスの大きさ、オフなら幅と高さの設定で描く
void scenario_custom_size(Checker &c)
{
	obs_source_t *canvas = create_stripes(nullptr);
	obs_source_t *custom = create_stripes([](obs_data_t *s) {
		obs_data_set_bool(s, "use_canvas_size", false);
		obs_data_set_int(s, "width", 50);
		obs_data_set_int(s, "height", 30);
	});
	Image image;
	if (!canvas || !custom || !render_settled(custom, image)) {
		c.expect(false, "failed to render the sources");
	} else {
		save_image("custom-size", image);
		c.expect(obs_source_get_width(canvas) == kCanvasWidth && obs_source_get_height(canvas) == kCanvasHeight,
			 "the canvas-sized source does not match the canvas");
		c.expect(image.width == 50 && image.height == 30, "the custom size is not used");
		expect_column_varies(c, image, image.width / 2);
	}
	obs_source_release(custom);
	obs_source_release(canvas);
}

struct Scenario {
	const char *name;
	std::function<void(Checker &)> run;
};

const Scenario scenarios[] = {
	{"horizontal", scenario_horizontal},
	{"vertical", scenario_vertical},
	{"rotation", scenario_rotation},
	{"period", scenario_period},
	{"softness", scenario_softness},
	{"softness-over-100", scenario_softness_over_100},
	{"defaults", scenario_defaults},
	{"transparent-background", scenario_transparent_background},
	{"still", scenario_still},
	{"flow", scenario_flow},
	{"custom-size", scenario_custom_size},
};

} // namespace

int main(int argc, char **argv)
{
	if (argc != 6) {
		std::cerr << "usage: render-test <plugin> <plugin data> <graphics module> <libobs data> <scenario>\n";
		return 2;
	}
	const Scenario *scenario = nullptr;
	for (const Scenario &s : scenarios) {
		if (std::strcmp(argv[5], s.name) == 0) {
			scenario = &s;
		}
	}
	if (!scenario) {
		std::cerr << "unknown scenario: " << argv[5] << '\n';
		return 2;
	}
	if (!obs_test::start(argv[3], argv[4], kCanvasWidth, kCanvasHeight)) {
		return 2;
	}
	register_test_source();
	int result = 2;
	if (obs_test::load_plugin(argv[1], argv[2])) {
		Checker checker;
		scenario->run(checker);
		result = checker.failures == 0 ? 0 : 1;
		if (result != 0) {
			std::cerr << checker.failures << " check(s) failed in " << scenario->name << '\n';
		} else {
			std::cout << "PASS " << scenario->name << '\n';
		}
	}
	obs_shutdown();
	return result;
}
