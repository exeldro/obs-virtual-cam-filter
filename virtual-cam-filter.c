#include <obs-module.h>

struct virtual_cam_filter_context {
	obs_source_t *source;
	obs_output_t *virtualCam;
	uint8_t *video_data;
	uint32_t video_linesize;
	video_t *video_output;
	bool output_active;
	uint32_t width;
	uint32_t height;
	gs_texrender_t *texrender;
	gs_stagesurf_t *stagesurface;
	struct obs_video_info ovi;
	uint64_t last_failed_start;
	bool restart;
};

static const char *virtual_cam_filter_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Virtual Camera";
}

struct video_frame {
	uint8_t *data[MAX_AV_PLANES];
	uint32_t linesize[MAX_AV_PLANES];
};

void virtual_cam_filter_offscreen_render(void *data, uint32_t cx, uint32_t cy)
{
	struct virtual_cam_filter_context *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->source);
	if (!target) {
		return;
	}

	const uint32_t width = obs_source_get_base_width(target);
	const uint32_t height = obs_source_get_base_height(target);
	if (!width || !height)
		return;

	gs_texrender_reset(filter->texrender);

	if (!gs_texrender_begin(filter->texrender, width, height))
		return;

	struct vec4 background;
	vec4_zero(&background);

	gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
	gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	obs_source_skip_video_filter(filter->source);

	gs_blend_state_pop();
	gs_texrender_end(filter->texrender);

	if (filter->width != width || filter->height != height) {

		gs_stagesurface_destroy(filter->stagesurface);
		filter->stagesurface =
			gs_stagesurface_create(width, height, GS_BGRA);

		struct video_output_info vi = {0};
		vi.format = VIDEO_FORMAT_BGRA;
		vi.width = width;
		vi.height = height;
		vi.fps_den = filter->ovi.fps_den;
		vi.fps_num = filter->ovi.fps_num;
		vi.cache_size = 16;
		vi.colorspace = VIDEO_CS_DEFAULT;
		vi.range = VIDEO_RANGE_DEFAULT;
		vi.name = obs_source_get_name(filter->source);

		video_output_close(filter->video_output);
		filter->video_output = NULL;
		if (video_output_open(&filter->video_output, &vi) ==
		    VIDEO_OUTPUT_SUCCESS) {
			filter->width = width;
			filter->height = height;
			filter->restart = true;
		}
	}
	if (!filter->video_output || video_output_stopped(filter->video_output))
		return;

	if (!filter->output_active && obs_source_enabled(filter->source)) {
		const uint64_t time = obs_get_video_frame_time();
		if (time - filter->last_failed_start < 1000000)
			return;
		obs_output_set_media(filter->virtualCam, filter->video_output,
				     NULL);
		if (obs_output_start(filter->virtualCam)) {
			filter->output_active = true;
		} else {
			filter->last_failed_start = time;
		}

	} else if (filter->output_active &&
		   !obs_source_enabled(filter->source)) {
		obs_output_stop(filter->virtualCam);
		filter->output_active = false;
	}

	struct video_frame output_frame;
	if (!video_output_lock_frame(filter->video_output, &output_frame, 1,
				     obs_get_video_frame_time()))
		return;

	if (filter->video_data) {
		gs_stagesurface_unmap(filter->stagesurface);
		filter->video_data = NULL;
	}
	if (!filter->stagesurface)
		filter->stagesurface =
			gs_stagesurface_create(width, height, GS_BGRA);

	gs_stage_texture(filter->stagesurface,
			 gs_texrender_get_texture(filter->texrender));
	gs_stagesurface_map(filter->stagesurface, &filter->video_data,
			    &filter->video_linesize);

	if (filter->video_data && filter->video_linesize) {
		const uint32_t linesize = output_frame.linesize[0];
		for (uint32_t i = 0; i < filter->height; ++i) {
			const uint32_t dst_offset = linesize * i;
			const uint32_t src_offset = filter->video_linesize * i;
			memcpy(output_frame.data[0] + dst_offset,
			       filter->video_data + src_offset, linesize);
		}
	}

	video_output_unlock_frame(filter->video_output);
}

static void virtual_cam_filter_source_update(void *data, obs_data_t *settings)
{
	struct virtual_cam_filter_context *context = data;
	obs_remove_main_render_callback(virtual_cam_filter_offscreen_render,
					context);
	obs_add_main_render_callback(virtual_cam_filter_offscreen_render,
				     context);
}

static void virtual_cam_filter_source_defaults(obs_data_t *settings) {}

static void *virtual_cam_filter_source_create(obs_data_t *settings,
					      obs_source_t *source)
{
	struct virtual_cam_filter_context *context =
		bzalloc(sizeof(struct virtual_cam_filter_context));
	context->source = source;
	context->virtualCam = obs_output_create(
		"virtualcam_output", "virtualcam_output_filter", NULL, NULL);
	context->texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
	obs_get_video_info(&context->ovi);
	virtual_cam_filter_source_update(context, settings);
	return context;
}

static void virtual_cam_filter_source_destroy(void *data)
{
	struct virtual_cam_filter_context *context = data;
	obs_output_stop(context->virtualCam);

	obs_remove_main_render_callback(virtual_cam_filter_offscreen_render,
					context);

	video_output_close(context->video_output);
	context->video_output = NULL;

	gs_stagesurface_unmap(context->stagesurface);
	gs_stagesurface_destroy(context->stagesurface);
	gs_texrender_destroy(context->texrender);
	bfree(context);
}

static void virtual_cam_filter_source_tick(void *data, float seconds)
{
	struct virtual_cam_filter_context *context = data;
	if (context->restart && context->output_active) {
		obs_output_stop(context->virtualCam);
		context->output_active = false;
		context->restart = false;
	} else if (!context->output_active && context->video_output &&
		   obs_source_enabled(context->source)) {
		uint64_t time = obs_get_video_frame_time();
		if (time - context->last_failed_start < 1000000)
			return;
		obs_output_set_media(context->virtualCam, context->video_output,
				     NULL);
		if (obs_output_start(context->virtualCam)) {
			context->output_active = true;
		} else {
			context->last_failed_start = time;
		}

	} else if (context->output_active &&
		   !obs_source_enabled(context->source)) {
		obs_output_stop(context->virtualCam);
		context->output_active = false;
	}
}

static obs_properties_t *virtual_cam_filter_source_properties(void *data)
{
	struct virtual_cam_filter_context *s = data;
	obs_properties_t *props = obs_properties_create();
	return props;
}

void virtual_cam_filter_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct virtual_cam_filter_context *context = data;
	obs_source_skip_video_filter(context->source);
}
static void virtual_cam_filter_source_filter_remove(void *data,
						    obs_source_t *parent)
{
}

uint32_t virtual_cam_filter_source_width(void *data)
{
	struct virtual_cam_filter_context *s = data;
	return s->width;
}

uint32_t virtual_cam_filter_source_height(void *data)
{
	struct virtual_cam_filter_context *s = data;
	return s->height;
}

struct obs_source_info virtual_cam_filter_info = {
	.id = "virtual_cam_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = virtual_cam_filter_source_get_name,
	.create = virtual_cam_filter_source_create,
	.destroy = virtual_cam_filter_source_destroy,
	.update = virtual_cam_filter_source_update,
	.get_defaults = virtual_cam_filter_source_defaults,
	.video_render = virtual_cam_filter_source_render,
	.video_tick = virtual_cam_filter_source_tick,
	.get_properties = virtual_cam_filter_source_properties,
	.filter_remove = virtual_cam_filter_source_filter_remove,
	.get_width = virtual_cam_filter_source_width,
	.get_height = virtual_cam_filter_source_height,
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("virtual-cam-filter", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Virtual Camera Filter";
}

bool obs_module_load(void)
{
	obs_register_source(&virtual_cam_filter_info);
	return true;
}
