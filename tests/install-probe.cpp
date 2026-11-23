// OBS Studio の中で動き、導入したプラグインを確かめる検査用のモジュール。
// 環境変数 GRADIENT_STRIPES_PROBE_RESULT_DIR と GRADIENT_STRIPES_PROBE_PHASE があるときだけ読み込まれる。
// - setup: 読み込みと登録、リソース、名前、画素、境界値を確かめ、設定を変えたソースをシーンに保存する
// - restore: 再起動後に、保存したソースと設定が残っていることと、名前と画素を確かめる
// 結果を result.json に書き、OBS を閉じる。配布物には含めない。

#include "render-fixture.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QTimer>

#include <cstring>
#include <functional>

OBS_DECLARE_MODULE()

namespace {

QString result_dir;
QString phase;
QString expected_name;
QJsonObject evidence;

constexpr const char *kModuleName = "simple-gradient-stripes";
constexpr const char *kSourceId = "simple_gradient_stripes_source";
constexpr const char *kSourceName = "Gradient stripes install check";

// 保存して再起動後に確かめる設定。OBS の色は 0xAABBGGRR で、不透明の赤
constexpr double kAngle = 45.0;
constexpr long long kSpacing = 64;
constexpr long long kStripeColor = 0xFF0000FF;
constexpr long long kSoftness = 0;

void write_settings(obs_data_t *settings)
{
	obs_data_set_double(settings, "angle", kAngle);
	obs_data_set_int(settings, "spacing", kSpacing);
	obs_data_set_double(settings, "speed", 0.0);
	obs_data_set_int(settings, "stripe_color", kStripeColor);
	obs_data_set_int(settings, "background_color", 0);
	obs_data_set_int(settings, "softness", kSoftness);
}

// 赤いしまと透明な地があり、45° の右上がりの対角線の上で画素が一定であることを確かめる。0 で合格
int check_pixels(obs_source_t *source)
{
	render_test::Image warmup, image;
	if (!render_test::render(source, warmup) || !render_test::render(source, image) || image.width < 2 ||
	    image.height < 2) {
		return 2;
	}
	size_t red = 0;
	size_t transparent = 0;
	for (uint32_t y = 0; y < image.height; ++y) {
		for (uint32_t x = 0; x < image.width; ++x) {
			const uint8_t *p = image.at(x, y);
			red += (p[3] == 255 && p[0] >= 250 && p[1] <= 5 && p[2] <= 5) ? 1 : 0;
			transparent += p[3] == 0 ? 1 : 0;
		}
	}
	size_t diagonal_mismatch = 0;
	const uint32_t y0 = image.height - 1;
	for (uint32_t x0 = 0; x0 < image.width; x0 += 17) {
		for (uint32_t k = 1; x0 + k < image.width && k <= y0; ++k) {
			diagonal_mismatch += std::abs(image.at(x0 + k, y0 - k)[3] - image.at(x0, y0)[3]) > 3 ? 1 : 0;
		}
	}
	evidence.insert("output_size", QString("%1x%2").arg(image.width).arg(image.height));
	evidence.insert("red_pixels", static_cast<qint64>(red));
	evidence.insert("transparent_pixels", static_cast<qint64>(transparent));
	evidence.insert("diagonal_mismatch", static_cast<qint64>(diagonal_mismatch));
	return red > 0 && transparent > 0 && diagonal_mismatch == 0 ? 0 : 1;
}

struct BoundaryCase {
	const char *name;
	std::function<void(obs_data_t *)> write;
	uint32_t expected_width;
	uint32_t expected_height;
};

// 範囲の端と範囲外の値で、作成・描画・大きさを確かめる。範囲外の値は範囲に収める
bool check_boundaries()
{
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	const uint32_t cw = ovi.base_width;
	const uint32_t ch = ovi.base_height;
	const BoundaryCase cases[] = {
		{"spacing-min", [](obs_data_t *s) { obs_data_set_int(s, "spacing", 4); }, cw, ch},
		{"spacing-max", [](obs_data_t *s) { obs_data_set_int(s, "spacing", 8192); }, cw, ch},
		{"spacing-below-range", [](obs_data_t *s) { obs_data_set_int(s, "spacing", 0); }, cw, ch},
		{"spacing-above-range", [](obs_data_t *s) { obs_data_set_int(s, "spacing", 100000); }, cw, ch},
		{"angle-360", [](obs_data_t *s) { obs_data_set_double(s, "angle", 360.0); }, cw, ch},
		{"speed-min", [](obs_data_t *s) { obs_data_set_double(s, "speed", 0.0); }, cw, ch},
		{"speed-below-range", [](obs_data_t *s) { obs_data_set_double(s, "speed", -500.0); }, cw, ch},
		{"speed-max", [](obs_data_t *s) { obs_data_set_double(s, "speed", 2000.0); }, cw, ch},
		{"softness-0", [](obs_data_t *s) { obs_data_set_int(s, "softness", 0); }, cw, ch},
		{"softness-200", [](obs_data_t *s) { obs_data_set_int(s, "softness", 200); }, cw, ch},
		{"softness-above-range", [](obs_data_t *s) { obs_data_set_int(s, "softness", 1000); }, cw, ch},
		{"size-min",
		 [](obs_data_t *s) {
			 obs_data_set_bool(s, "use_canvas_size", false);
			 obs_data_set_int(s, "width", 1);
			 obs_data_set_int(s, "height", 1);
		 },
		 1, 1},
		{"size-below-range",
		 [](obs_data_t *s) {
			 obs_data_set_bool(s, "use_canvas_size", false);
			 obs_data_set_int(s, "width", 0);
			 obs_data_set_int(s, "height", -5);
		 },
		 1, 1},
		{"size-max",
		 [](obs_data_t *s) {
			 obs_data_set_bool(s, "use_canvas_size", false);
			 obs_data_set_int(s, "width", 8192);
			 obs_data_set_int(s, "height", 8192);
		 },
		 8192, 8192},
		{"size-above-range",
		 [](obs_data_t *s) {
			 obs_data_set_bool(s, "use_canvas_size", false);
			 obs_data_set_int(s, "width", 99999);
			 obs_data_set_int(s, "height", 99999);
		 },
		 8192, 8192},
	};
	QJsonArray results;
	bool all = true;
	for (const BoundaryCase &c : cases) {
		obs_data_t *settings = obs_data_create();
		c.write(settings);
		obs_source_t *source = obs_source_create_private(kSourceId, c.name, settings);
		obs_data_release(settings);
		render_test::Image image;
		const bool rendered = source && render_test::render(source, image);
		const bool size_ok = rendered && image.width == c.expected_width && image.height == c.expected_height;
		obs_source_release(source);
		results.append(QJsonObject{{"case", c.name},
					   {"rendered", rendered},
					   {"size", QString("%1x%2").arg(image.width).arg(image.height)},
					   {"passed", size_ok}});
		all = all && size_ok;
	}
	evidence.insert("boundaries", results);
	return all;
}

void finish(bool passed, const QString &reason)
{
	QJsonObject result{{"passed", passed},
			   {"reason", reason},
			   {"obs", obs_get_version_string()},
			   {"phase", phase},
			   {"evidence", evidence}};
	if (auto *module = obs_get_module(kModuleName))
		result.insert("product_binary_path", QString::fromUtf8(obs_get_module_binary_path(module)));
	QFile file(result_dir + "/result.json");
	const auto json = QJsonDocument(result).toJson();
	if (!file.open(QIODevice::WriteOnly) || file.write(json) != json.size() || !file.flush())
		passed = false;
	file.close();
	blog(passed ? LOG_INFO : LOG_ERROR, "GRADIENT_STRIPES_PROBE %s phase=%s %s", passed ? "PASS" : "FAIL",
	     phase.toUtf8().constData(), reason.toUtf8().constData());
	auto *window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QTimer::singleShot(0, window, [window] { window->close(); });
}

void inspect()
{
	auto *module = obs_get_module(kModuleName);
	if (!module) {
		finish(false, "product module missing");
		return;
	}
	int registrations = 0;
	const char *id = nullptr;
	for (size_t i = 0; obs_enum_input_types(i, &id); ++i)
		registrations += std::strcmp(id, kSourceId) == 0;
	evidence.insert("source_registrations", registrations);
	if (registrations != 1) {
		finish(false, "source registration count mismatch");
		return;
	}
	char *effect = obs_find_module_file(module, "gradient-stripes.effect");
	char *locale = obs_find_module_file(module, "locale/ja-JP.ini");
	const QString translated = QString::fromUtf8(obs_module_get_locale_text(module, "GradientStripes"));
	evidence.insert("effect_path", effect ? QString::fromUtf8(effect) : QString());
	evidence.insert("locale_path", locale ? QString::fromUtf8(locale) : QString());
	evidence.insert("source_name", translated);
	const bool resources = effect && locale && translated == expected_name;
	bfree(effect);
	bfree(locale);
	if (!resources) {
		finish(false, "installed effect or translated locale unavailable");
		return;
	}
	obs_source_t *source = nullptr;
	if (phase == "setup") {
		if (!check_boundaries()) {
			finish(false, "boundary value check failed");
			return;
		}
		obs_data_t *settings = obs_data_create();
		write_settings(settings);
		source = obs_source_create(kSourceId, kSourceName, settings, nullptr);
		obs_data_release(settings);
		auto *scene_source = obs_frontend_get_current_scene();
		auto *scene = scene_source ? obs_scene_from_source(scene_source) : nullptr;
		const bool attached = source && scene && obs_scene_add(scene, source);
		obs_source_release(scene_source);
		if (!attached) {
			obs_source_release(source);
			finish(false, "source or scene attachment failed");
			return;
		}
	} else {
		source = obs_get_source_by_name(kSourceName);
	}
	if (!source || std::strcmp(obs_source_get_id(source), kSourceId) != 0) {
		obs_source_release(source);
		finish(false, "persisted source missing");
		return;
	}
	auto *settings = obs_source_get_settings(source);
	const bool retained = obs_data_get_double(settings, "angle") == kAngle &&
			      obs_data_get_int(settings, "spacing") == kSpacing &&
			      obs_data_get_int(settings, "stripe_color") == kStripeColor &&
			      obs_data_get_int(settings, "softness") == kSoftness;
	evidence.insert("source_settings", QJsonDocument::fromJson(obs_data_get_json(settings)).object());
	obs_data_release(settings);
	const int pixels = retained ? check_pixels(source) : -1;
	evidence.insert("pixel_result", pixels);
	obs_source_release(source);
	if (retained && pixels == 0)
		obs_frontend_save();
	finish(retained && pixels == 0, "installed resources, boundaries, persisted settings and pixels checked");
}

void frontend_event(obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
		QTimer::singleShot(1000, static_cast<QMainWindow *>(obs_frontend_get_main_window()), inspect);
}

} // namespace

bool obs_module_load()
{
	result_dir = qEnvironmentVariable("GRADIENT_STRIPES_PROBE_RESULT_DIR");
	phase = qEnvironmentVariable("GRADIENT_STRIPES_PROBE_PHASE");
	expected_name = qEnvironmentVariable("GRADIENT_STRIPES_PROBE_EXPECTED_NAME");
	if (result_dir.isEmpty() || (phase != "setup" && phase != "restore") || expected_name.isEmpty())
		return false;
	obs_frontend_add_event_callback(frontend_event, nullptr);
	return true;
}

void obs_module_unload()
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
}
