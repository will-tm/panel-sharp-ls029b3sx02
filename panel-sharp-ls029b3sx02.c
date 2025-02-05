// SPDX-License-Identifier: GPL-2.0-only

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>


struct sharp_panel {
    struct drm_panel base;
    struct mipi_dsi_device *dsi;

    struct gpio_desc *bl_en_gpio;
    struct gpio_desc *reset_gpio;

    bool prepared;
};

static inline struct sharp_panel *to_sharp_panel(struct drm_panel *panel)
{
    return container_of(panel, struct sharp_panel, base);
}

static void sharp_panel_reset(struct sharp_panel *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int sharp_panel_on(struct sharp_panel *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	msleep(50);

	return 0;
}

static void sharp_panel_off(struct sharp_panel *ctx)
{
    struct mipi_dsi_device *dsi = ctx->dsi;
    struct device *dev = &ctx->dsi->dev;
    int ret;

    dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

    gpiod_set_value_cansleep(ctx->bl_en_gpio, 0);

    ret = mipi_dsi_dcs_set_display_off(dsi);
    if (ret < 0)
        dev_err(dev, "failed to set display off: %d\n", ret);

    ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
    if (ret < 0)
        dev_err(dev, "failed to enter sleep mode: %d\n", ret);

    msleep(100);
}

static int sharp_panel_unprepare(struct drm_panel *panel)
{
    struct sharp_panel *ctx = to_sharp_panel(panel);

    if (!ctx->prepared)
        return 0;

    sharp_panel_off(ctx);
    gpiod_set_value_cansleep(ctx->reset_gpio, 1);
    ctx->prepared = false;

    return 0;
}

static int sharp_panel_prepare(struct drm_panel *panel)
{
    struct sharp_panel *ctx = to_sharp_panel(panel);
    struct device *dev = &ctx->dsi->dev;
    int ret;

    if (ctx->prepared) {
        return 0;
	}

    gpiod_set_value_cansleep(ctx->bl_en_gpio, 1);

	sharp_panel_reset(ctx);

    ret = sharp_panel_on(ctx);
    if (ret < 0) {
        dev_err(dev, "failed to set panel on: %d\n", ret);
        goto poweroff;
    }

    ctx->prepared = true;

    return 0;

poweroff:

    gpiod_set_value_cansleep(ctx->reset_gpio, 1);
    return ret;
}

static const struct drm_display_mode default_mode = {
    .clock = (1440 + 38 + 2 + 22) * (1440 + 8 + 2 + 6) * 60 / 1000,

    .hdisplay = 1440,
    .hsync_start = 1440 + 38,
    .hsync_end = 1440 + 38 + 2,
    .htotal = 1440 + 38 + 2 + 22,

    .vdisplay = 1440,
    .vsync_start = 1440 + 8,
    .vsync_end = 1440 + 8 + 2,
    .vtotal = 1440 + 8 + 2 + 6,

	.width_mm = 51,
	.height_mm = 51,
};

static int sharp_panel_get_modes(struct drm_panel *panel,
                   struct drm_connector *connector)
{
    struct drm_display_mode *mode;
    struct sharp_panel *ctx = to_sharp_panel(panel);
    struct device *dev = &ctx->dsi->dev;

    mode = drm_mode_duplicate(connector->dev, &default_mode);
    if (!mode) {
        dev_err(dev, "failed to add mode %ux%ux@%u\n",
            default_mode.hdisplay, default_mode.vdisplay,
            drm_mode_vrefresh(&default_mode));
        return -ENOMEM;
    }

    drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

    return 1;
}


static const struct drm_panel_funcs sharp_panel_funcs = {
    .unprepare = sharp_panel_unprepare,
    .prepare = sharp_panel_prepare,
    .get_modes = sharp_panel_get_modes,
};

static const struct of_device_id sharp_of_match[] = {
    { .compatible = "sharp,ls029b3sx02", },
    { }
};
MODULE_DEVICE_TABLE(of, sharp_of_match);

static int sharp_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct sharp_panel *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->bl_en_gpio = devm_gpiod_get(dev, "bl-en", GPIOD_OUT_LOW);
    if (IS_ERR(ctx->bl_en_gpio)) {
        ret = PTR_ERR(ctx->bl_en_gpio);
        dev_err(dev, "cannot get enable-gpio %d\n", ret);
        return ret;
    }

    ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->reset_gpio)) {
        ret = PTR_ERR(ctx->reset_gpio);
        dev_err(dev, "cannot get reset-gpios %d\n", ret);
        return ret;
    }

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
		MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

    ctx->base.prepare_prev_first = true;
	drm_panel_init(&ctx->base, dev, &sharp_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->base);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->base);
		return ret;
	}

	return 0;
}

static void sharp_panel_remove(struct mipi_dsi_device *dsi)
{
    struct sharp_panel *ctx = mipi_dsi_get_drvdata(dsi);
    int ret;

    ret = mipi_dsi_detach(dsi);
    if (ret < 0)
        dev_err(&dsi->dev, "failed to detach from DSI host: %d\n",
            ret);

    drm_panel_remove(&ctx->base);
}

static struct mipi_dsi_driver sharp_panel_driver = {
    .probe = sharp_panel_probe,
    .remove = sharp_panel_remove,
    .driver = {
        .name = "panel-sharp-ls029b3sx02",
        .of_match_table = sharp_of_match,
    },
};
module_mipi_dsi_driver(sharp_panel_driver);

MODULE_AUTHOR("William Markezana <william.markezana@gmail.com");
MODULE_DESCRIPTION("Sharp LS029B3SX02 DRM Panel Driver");
MODULE_LICENSE("GPL v2");
