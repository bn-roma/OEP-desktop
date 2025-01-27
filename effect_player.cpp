#include "effect_player.hpp"

#include <iostream>
#include <thread>
#include <optional>
#include <iostream>

namespace bnb::oep
{

    /* effect_player::create */
    effect_player_sptr interfaces::effect_player::create(const std::vector<std::string>& path_to_resources, const std::string& client_token)
    {
        // This particular example relies on OpenGL, so it should be explicitly requested
        bnb::interfaces::effect_player::set_render_backend(::bnb::interfaces::render_backend_type::opengl);

        return std::make_shared<bnb::oep::effect_player>(path_to_resources, client_token);
    }

    /* effect_player::effect_player CONSTRUCTOR */
    effect_player::effect_player(const std::vector<std::string>& path_to_resources, const std::string& client_token)
        : m_utility(path_to_resources, client_token)
        // the description of the passed parameters can be found at the link:
        // https://docs.banuba.com/face-ar-sdk/generated/doxygen/html/structbnb_1_1interfaces_1_1effect__player__configuration.html#a810709129e2bc13eae190305861345ce
        // Frame buffer for effect player is passed as 1x1 later on surface_created or surface_changed
        // it will be set to the actual surface size.
        // NOTE: these parameters explicitly influence performance, for instance, you have a small screen, e.g. 6 inches,
        // and your rendering surface is large, e.g. 4K, then it is not necessary to render effect in 4K resolution
        // since such precision will not be seen on the screen, so performance can be improved.
        // In the sample effect frame buffer and the surface are synced in surface_created and surface_changed
        , m_ep(bnb::interfaces::effect_player::create({
            1, // fx_width - the effect's framebuffer width
            1, // fx_height - the effect's framebuffer height
            bnb::interfaces::nn_mode::automatically,
            bnb::interfaces::face_search_mode::good,
            false,
            false}))
    {
    }

    /* effect_player::~effect_player */
    effect_player::~effect_player()
    {
    }

    /* effect_player::surface_created */
    void effect_player::surface_created(int32_t width, int32_t height)
    {
        m_ep->surface_created(width, height);
        // Set explicitly the framebuffer of Effect Player to sync with surface size
        m_ep->effect_manager()->set_effect_size(width, height);
    }

    /* effect_player::surface_changed */
    void effect_player::surface_changed(int32_t width, int32_t height)
    {
        m_ep->surface_changed(width, height);
        // Set explicitly the framebuffer of Effect Player to sync with surface size
        m_ep->effect_manager()->set_effect_size(width, height);
    }

    /* effect_player::surface_destroyed */
    void effect_player::surface_destroyed()
    {
        m_ep->surface_destroyed();
    }

    /* effect_player::load_effect */
    bool effect_player::load_effect(const std::string& effect)
    {
        if (auto effect_manager = m_ep->effect_manager()) {
            effect_manager->load(effect);
            return true;
        }
        return false;
    }

    /* effect_player::call_js_method */
    bool effect_player::call_js_method(const std::string& method, const std::string& param)
    {
        if (auto e_manager = m_ep->effect_manager()) {
            if (auto effect = e_manager->current()) {
                effect->call_js_method(method, param);
            } else {
                std::cout << "[Error] effect not loaded" << std::endl;
                return false;
            }
        } else {
            std::cout << "[Error] effect manager not initialized" << std::endl;
            return false;
        }
        return true;
    }

    /* effect_player::pause */
    void effect_player::pause()
    {
        m_ep->playback_pause();
    }

    /* effect_player::resume */
    void effect_player::resume()
    {
        m_ep->playback_play();
    }

    /* effect_player::stop */
    void effect_player::stop()
    {
        m_ep->playback_stop();
    }

    /* effect_player::push_frame */
    void effect_player::push_frame(pixel_buffer_sptr image, bnb::oep::interfaces::rotation image_orientation)
    {
        using ns = bnb::oep::interfaces::image_format;
        auto bnb_image_format = make_bnb_image_format(image, image_orientation);
        switch (image->get_image_format()) {
            case ns::bpc8_rgb:
            case ns::bpc8_bgr:
            case ns::bpc8_rgba:
            case ns::bpc8_bgra:
            case ns::bpc8_argb:
                m_ep->push_frame(
                    full_image_t(bpc8_image_t(
                        color_plane(image->get_base_sptr()),
                        make_bnb_pixel_format(image),
                        bnb_image_format)));
                break;
            case ns::nv12_bt601_full:
            case ns::nv12_bt601_video:
            case ns::nv12_bt709_full:
            case ns::nv12_bt709_video:
                m_ep->push_frame(
                    full_image_t(yuv_image_t(
                        color_plane(image->get_base_sptr_of_plane(0)),
                        color_plane(image->get_base_sptr_of_plane(1)),
                        bnb_image_format,
                        make_bnb_yuv_format(image))));
                break;
            case ns::i420_bt601_full:
            case ns::i420_bt601_video:
            case ns::i420_bt709_full:
            case ns::i420_bt709_video:
                m_ep->push_frame(
                    full_image_t(yuv_image_t(
                        color_plane(image->get_base_sptr_of_plane(0)),
                        color_plane(image->get_base_sptr_of_plane(1)),
                        color_plane(image->get_base_sptr_of_plane(2)),
                        bnb_image_format,
                        make_bnb_yuv_format(image))));
                break;
            default:
                break;
        }
    }

    /* effect_player::draw */
    void effect_player::draw()
    {
        while (m_ep->draw() < 0) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    /* effect_player::make_bnb_image_format */
    bnb::image_format effect_player::make_bnb_image_format(pixel_buffer_sptr image, interfaces::rotation orientation)
    {
        bnb::camera_orientation camera_orient {bnb::camera_orientation::deg_0};

        using ns = bnb::oep::interfaces::rotation;
        switch (orientation) {
            case ns::deg0:
                break;
            case ns::deg90:
                camera_orient = bnb::camera_orientation::deg_90;
                break;
            case ns::deg180:
                camera_orient = bnb::camera_orientation::deg_180;
                break;
                break;
            case ns::deg270:
                camera_orient = bnb::camera_orientation::deg_270;
                break;
        }

        return {static_cast<uint32_t>(image->get_width()), static_cast<uint32_t>(image->get_height()), camera_orient, false, 0, std::nullopt};
    }

    /* effect_player::make_bnb_yuv_format */
    bnb::yuv_format_t effect_player::make_bnb_yuv_format(pixel_buffer_sptr image)
    {
        bnb::yuv_format format {bnb::yuv_format::yuv_nv12};  /* i.e. NV12 or I420 */
        bnb::color_std standard {bnb::color_std::bt601}; /* i.e. BT.601 or BT.709 */
        bnb::color_range range {bnb::color_range::full}; /* i.e. "full" or "video" */

        using ns = bnb::oep::interfaces::image_format;
        switch (image->get_image_format()) {
            case ns::nv12_bt601_full:
                break;
            case ns::nv12_bt601_video:
                range = bnb::color_range::video;
                break;
            case ns::nv12_bt709_full:
                standard = bnb::color_std::bt709;
                break;
            case ns::nv12_bt709_video:
                range = bnb::color_range::video;
                standard = bnb::color_std::bt709;
                break;
            case ns::i420_bt601_full:
                format = bnb::yuv_format::yuv_i420;
                break;
            case ns::i420_bt601_video:
                range = bnb::color_range::video;
                format = bnb::yuv_format::yuv_i420;
                break;
            case ns::i420_bt709_full:
                format = bnb::yuv_format::yuv_i420;
                standard = bnb::color_std::bt709;
                break;
            case ns::i420_bt709_video:
                range = bnb::color_range::video;
                format = bnb::yuv_format::yuv_i420;
                standard = bnb::color_std::bt709;
                break;
            default:
                break;
        }

        return {range, standard, format};
    }

    /* effect_player::make_bnb_pixel_format */
    bnb::interfaces::pixel_format effect_player::make_bnb_pixel_format(pixel_buffer_sptr image)
    {
        bnb::interfaces::pixel_format fmt {bnb::interfaces::pixel_format::rgb};

        using ns = bnb::oep::interfaces::image_format;
        switch (image->get_image_format()) {
            case ns::bpc8_rgb:
                break;
            case ns::bpc8_bgr:
                fmt = bnb::interfaces::pixel_format::bgr;
                break;
            case ns::bpc8_rgba:
                fmt = bnb::interfaces::pixel_format::rgba;
                break;
            case ns::bpc8_bgra:
                fmt = bnb::interfaces::pixel_format::bgra;
                break;
            case ns::bpc8_argb:
                fmt = bnb::interfaces::pixel_format::argb;
                break;
            default:
                break;
        }
        return fmt;
    }

} /* namespace bnb::oep */
