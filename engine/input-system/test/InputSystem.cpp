#include "core/InputSystem.hpp"
#include <SDL3/SDL.h>
#include <gtest/gtest.h>
#include <initializer_list>

namespace
{
    SDL_Event keyEvent(const uint32_t type, const SDL_Scancode scancode, const bool repeat = false)
    {
        SDL_Event event{};
        event.type = type;
        event.key.scancode = scancode;
        event.key.repeat = repeat;
        return event;
    }

    SDL_Event mouseButtonEvent(const uint32_t type, const uint8_t button)
    {
        SDL_Event event{};
        event.type = type;
        event.button.button = button;
        return event;
    }

    class InputSystemTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            ASSERT_TRUE(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD));
        }

        static void TearDownTestSuite()
        {
            SDL_Quit();
        }

        void SetUp() override
        {
            SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        }

        void TearDown() override
        {
            SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        }

        static void pushEvents(core::InputSystem& input, std::initializer_list<SDL_Event> events)
        {
            for(auto event : events) {
                ASSERT_TRUE(SDL_PushEvent(&event));
            }
            SDL_Event event{};
            while(SDL_PollEvent(&event)) {
                input.processEvent(event);
            }
            input.update();
        }

        static void initialize(core::InputSystem& input)
        {
            ASSERT_TRUE(input.initialize());
            input.resetKeyboardAndMouse();
            input.update();
        }
    };
}

TEST_F(InputSystemTest, PublishesHeldPressedReleasedAndRepeatState)
{
    core::InputSystem input;
    ASSERT_NO_FATAL_FAILURE(initialize(input));

    pushEvents(input, { keyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A) });
    EXPECT_TRUE(input.isKeyDown(core::Key::a));
    EXPECT_TRUE(input.wasKeyPressed(core::Key::a));
    EXPECT_FALSE(input.wasKeyReleased(core::Key::a));

    pushEvents(input, { keyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A, true) });
    EXPECT_TRUE(input.isKeyDown(core::Key::a));
    EXPECT_FALSE(input.wasKeyPressed(core::Key::a));

    pushEvents(input, { keyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_A) });
    EXPECT_FALSE(input.isKeyDown(core::Key::a));
    EXPECT_TRUE(input.wasKeyReleased(core::Key::a));
}

TEST_F(InputSystemTest, KeepsBothEdgesForSameFrameTransitions)
{
    core::InputSystem input;
    ASSERT_NO_FATAL_FAILURE(initialize(input));

    pushEvents(input, {
        keyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE),
        keyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE),
    });

    EXPECT_FALSE(input.isKeyDown(core::Key::space));
    EXPECT_TRUE(input.wasKeyPressed(core::Key::space));
    EXPECT_TRUE(input.wasKeyReleased(core::Key::space));

    SDL_Event mouse_down = mouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_X1);
    mouse_down.button.x = 12.5f;
    mouse_down.button.y = 30.0f;
    SDL_Event mouse_up = mouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_X1);
    mouse_up.button.x = 12.5f;
    mouse_up.button.y = 30.0f;
    pushEvents(input, { mouse_down, mouse_up });

    EXPECT_FALSE(input.isMouseButtonDown(core::MouseButton::x1));
    EXPECT_TRUE(input.wasMouseButtonPressed(core::MouseButton::x1));
    EXPECT_TRUE(input.wasMouseButtonReleased(core::MouseButton::x1));
    EXPECT_FLOAT_EQ(input.getMouseState().x, 12.5f);
    EXPECT_FLOAT_EQ(input.getMouseState().y, 30.0f);
}

TEST_F(InputSystemTest, MapsPhysicalScancodesIndependentlyOfKeycodes)
{
    core::InputSystem input;
    ASSERT_NO_FATAL_FAILURE(initialize(input));
    SDL_Event event = keyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A);
    event.key.key = SDLK_ESCAPE;
    pushEvents(input, { event });

    EXPECT_TRUE(input.isKeyDown(core::Key::a));
    EXPECT_FALSE(input.isKeyDown(core::Key::q));
}

TEST_F(InputSystemTest, FocusLossReleasesHeldKeyboardAndMouseButtons)
{
    core::InputSystem input;
    ASSERT_NO_FATAL_FAILURE(initialize(input));
    pushEvents(input, {
        keyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_LSHIFT),
        mouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT),
    });

    SDL_Event focus_lost{};
    focus_lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    pushEvents(input, { focus_lost });

    EXPECT_FALSE(input.isKeyDown(core::Key::left_shift));
    EXPECT_TRUE(input.wasKeyReleased(core::Key::left_shift));
    EXPECT_FALSE(input.isMouseButtonDown(core::MouseButton::left));
    EXPECT_TRUE(input.wasMouseButtonReleased(core::MouseButton::left));
}

TEST_F(InputSystemTest, AccumulatesMouseMotionAndTwoAxisWheel)
{
    core::InputSystem input;
    ASSERT_NO_FATAL_FAILURE(initialize(input));

    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.x = 20.5f;
    motion.motion.y = 30.25f;
    motion.motion.xrel = 3.0f;
    motion.motion.yrel = -2.0f;
    const SDL_Event first_motion = motion;
    motion.motion.xrel = 4.0f;
    motion.motion.yrel = 1.0f;

    SDL_Event wheel{};
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.x = 0.5f;
    wheel.wheel.y = 1.25f;
    wheel.wheel.mouse_x = 44.0f;
    wheel.wheel.mouse_y = 55.0f;
    SDL_Event flipped_wheel = wheel;
    flipped_wheel.wheel.x = 0.25f;
    flipped_wheel.wheel.y = -0.5f;
    flipped_wheel.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    pushEvents(input, { first_motion, motion, wheel, flipped_wheel });

    const auto state = input.getMouseState();
    EXPECT_FLOAT_EQ(state.x, 44.0f);
    EXPECT_FLOAT_EQ(state.y, 55.0f);
    EXPECT_FLOAT_EQ(state.delta_x, 7.0f);
    EXPECT_FLOAT_EQ(state.delta_y, -1.0f);
    EXPECT_FLOAT_EQ(state.wheel_x, 0.25f);
    EXPECT_FLOAT_EQ(state.wheel_y, 1.75f);
}

TEST_F(InputSystemTest, GameplayCaptureDoesNotChangeRawState)
{
    core::InputSystem input;
    ASSERT_NO_FATAL_FAILURE(initialize(input));
    pushEvents(input, {
        keyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_B),
        mouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_RIGHT),
    });

    input.setGameplayCapture(true, true);
    EXPECT_FALSE(input.isKeyDown(core::Key::b));
    EXPECT_FALSE(input.wasKeyPressed(core::Key::b));
    EXPECT_FALSE(input.isMouseButtonDown(core::MouseButton::right));
    EXPECT_TRUE(input.isRawKeyDown(core::Key::b));
    EXPECT_TRUE(input.wasRawKeyPressed(core::Key::b));
    EXPECT_TRUE(input.isRawMouseButtonDown(core::MouseButton::right));

    input.setGameplayCapture(false, false);
    EXPECT_TRUE(input.isKeyDown(core::Key::b));
    EXPECT_TRUE(input.isMouseButtonDown(core::MouseButton::right));
}

TEST_F(InputSystemTest, MapsMouseForStretchAspectRatioAndIntegerScaling)
{
    const core::Vector2U canvas{ 320, 240 };

    const auto stretched = core::mapMouseToCanvas(
        { 320.0f, 180.0f }, { 640, 360 }, canvas, core::CanvasScalingMode::stretch);
    EXPECT_FLOAT_EQ(stretched.position.x, 160.0f);
    EXPECT_FLOAT_EQ(stretched.position.y, 120.0f);
    EXPECT_TRUE(stretched.inside);

    const auto letterboxed = core::mapMouseToCanvas(
        { 320.0f, 180.0f }, { 640, 360 }, canvas, core::CanvasScalingMode::aspect_ratio);
    EXPECT_FLOAT_EQ(letterboxed.position.x, 160.0f);
    EXPECT_FLOAT_EQ(letterboxed.position.y, 120.0f);
    EXPECT_TRUE(letterboxed.inside);

    const auto outside = core::mapMouseToCanvas(
        { 20.0f, 180.0f }, { 640, 360 }, canvas, core::CanvasScalingMode::aspect_ratio);
    EXPECT_FALSE(outside.inside);

    const auto integer_scaled = core::mapMouseToCanvas(
        { 400.0f, 300.0f }, { 800, 600 }, canvas, core::CanvasScalingMode::integer_aspect_ratio);
    EXPECT_FLOAT_EQ(integer_scaled.position.x, 160.0f);
    EXPECT_FLOAT_EQ(integer_scaled.position.y, 120.0f);
    EXPECT_TRUE(integer_scaled.inside);

    const auto integer_letterboxed = core::mapMouseToCanvas(
        { 50.0f, 300.0f }, { 1000, 600 }, canvas, core::CanvasScalingMode::integer_aspect_ratio);
    EXPECT_FALSE(integer_letterboxed.inside);
}

TEST_F(InputSystemTest, TracksVirtualJoystickAndKeepsStaleIdsSafe)
{
    core::InputSystem input;
    ASSERT_NO_FATAL_FAILURE(initialize(input));

    SDL_VirtualJoystickDesc descriptor{};
    SDL_INIT_INTERFACE(&descriptor);
    descriptor.type = SDL_JOYSTICK_TYPE_FLIGHT_STICK;
    descriptor.name = "LuaSTG test joystick";
    descriptor.naxes = 1;
    descriptor.nbuttons = 1;
    descriptor.nhats = 1;
    const SDL_JoystickID id = SDL_AttachVirtualJoystick(&descriptor);
    ASSERT_NE(id, 0u);

    SDL_Event added{};
    added.type = SDL_EVENT_JOYSTICK_ADDED;
    added.jdevice.which = id;
    input.processEvent(added);
    ASSERT_TRUE(input.isJoystickConnected(id));

    SDL_Joystick* const joystick = SDL_GetJoystickFromID(id);
    ASSERT_NE(joystick, nullptr);
    ASSERT_TRUE(SDL_SetJoystickVirtualAxis(joystick, 0, SDL_JOYSTICK_AXIS_MIN));
    ASSERT_TRUE(SDL_SetJoystickVirtualButton(joystick, 0, true));
    ASSERT_TRUE(SDL_SetJoystickVirtualHat(joystick, 0, SDL_HAT_RIGHTUP));
    SDL_UpdateJoysticks();

    SDL_Event axis{};
    axis.type = SDL_EVENT_JOYSTICK_AXIS_MOTION;
    axis.jaxis.which = id;
    axis.jaxis.axis = 0;
    axis.jaxis.value = SDL_JOYSTICK_AXIS_MIN;
    input.processEvent(axis);

    SDL_Event button{};
    button.type = SDL_EVENT_JOYSTICK_BUTTON_DOWN;
    button.jbutton.which = id;
    button.jbutton.button = 0;
    input.processEvent(button);

    SDL_Event hat{};
    hat.type = SDL_EVENT_JOYSTICK_HAT_MOTION;
    hat.jhat.which = id;
    hat.jhat.hat = 0;
    hat.jhat.value = SDL_HAT_RIGHTUP;
    input.processEvent(hat);
    input.update();

    EXPECT_FLOAT_EQ(input.getJoystickAxis(id, 0), -1.0f);
    EXPECT_TRUE(input.isJoystickButtonDown(id, 0));
    EXPECT_TRUE(input.wasJoystickButtonPressed(id, 0));
    EXPECT_EQ(
        static_cast<uint8_t>(input.getJoystickHat(id, 0)),
        static_cast<uint8_t>(core::JoystickHat::up) | static_cast<uint8_t>(core::JoystickHat::right));

    axis.jaxis.value = SDL_JOYSTICK_AXIS_MAX;
    input.processEvent(axis);
    button.type = SDL_EVENT_JOYSTICK_BUTTON_UP;
    input.processEvent(button);
    input.update();
    EXPECT_FLOAT_EQ(input.getJoystickAxis(id, 0), 1.0f);
    EXPECT_FALSE(input.isJoystickButtonDown(id, 0));
    EXPECT_TRUE(input.wasJoystickButtonReleased(id, 0));

    SDL_Event removed{};
    removed.type = SDL_EVENT_JOYSTICK_REMOVED;
    removed.jdevice.which = id;
    input.processEvent(removed);
    EXPECT_FALSE(input.isJoystickConnected(id));
    EXPECT_EQ(input.getJoystickButtonCount(id), 0u);
    EXPECT_FLOAT_EQ(input.getJoystickAxis(id, 0), 0.0f);
    EXPECT_EQ(input.getJoystickHat(id, 0), core::JoystickHat::centered);

    EXPECT_TRUE(SDL_DetachVirtualJoystick(id));
    SDL_FlushEvents(SDL_EVENT_JOYSTICK_ADDED, SDL_EVENT_JOYSTICK_UPDATE_COMPLETE);
}

TEST_F(InputSystemTest, TracksMappedVirtualGamepadEndpointsAndUnsupportedRumble)
{
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    SDL_VirtualJoystickDesc descriptor{};
    SDL_INIT_INTERFACE(&descriptor);
    descriptor.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    descriptor.name = "LuaSTG test gamepad";
    descriptor.naxes = SDL_GAMEPAD_AXIS_COUNT;
    descriptor.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    descriptor.button_mask = UINT32_C(1) << SDL_GAMEPAD_BUTTON_SOUTH;
    descriptor.axis_mask = (UINT32_C(1) << SDL_GAMEPAD_AXIS_LEFTX) | (UINT32_C(1) << SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    const SDL_JoystickID id = SDL_AttachVirtualJoystick(&descriptor);
    ASSERT_NE(id, 0u);

    core::InputSystem input;
    ASSERT_NO_FATAL_FAILURE(initialize(input));

    SDL_Event added{};
    added.type = SDL_EVENT_GAMEPAD_ADDED;
    added.gdevice.which = id;
    input.processEvent(added);
    ASSERT_TRUE(input.isGamepadConnected(id));

    SDL_Event button_down{};
    button_down.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    button_down.gbutton.which = id;
    button_down.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    input.processEvent(button_down);

    SDL_Event left_axis{};
    left_axis.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    left_axis.gaxis.which = id;
    left_axis.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    left_axis.gaxis.value = SDL_JOYSTICK_AXIS_MIN;
    input.processEvent(left_axis);

    SDL_Event trigger{};
    trigger.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    trigger.gaxis.which = id;
    trigger.gaxis.axis = SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
    trigger.gaxis.value = SDL_JOYSTICK_AXIS_MAX;
    input.processEvent(trigger);
    input.update();

    EXPECT_TRUE(input.isGamepadButtonDown(id, core::GamepadButton::south));
    EXPECT_TRUE(input.wasGamepadButtonPressed(id, core::GamepadButton::south));
    EXPECT_FLOAT_EQ(input.getGamepadAxis(id, core::GamepadAxis::left_x), -1.0f);
    EXPECT_FLOAT_EQ(input.getGamepadAxis(id, core::GamepadAxis::left_trigger), 1.0f);
    EXPECT_FALSE(input.rumbleGamepad(id, 1.0f, 1.0f, 10));

    SDL_Event button_up{};
    button_up.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    button_up.gbutton.which = id;
    button_up.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    input.processEvent(button_up);
    left_axis.gaxis.value = SDL_JOYSTICK_AXIS_MAX;
    input.processEvent(left_axis);
    input.update();
    EXPECT_FALSE(input.isGamepadButtonDown(id, core::GamepadButton::south));
    EXPECT_TRUE(input.wasGamepadButtonReleased(id, core::GamepadButton::south));
    EXPECT_FLOAT_EQ(input.getGamepadAxis(id, core::GamepadAxis::left_x), 1.0f);

    SDL_Event removed{};
    removed.type = SDL_EVENT_GAMEPAD_REMOVED;
    removed.gdevice.which = id;
    input.processEvent(removed);
    EXPECT_FALSE(input.isGamepadConnected(id));
    EXPECT_FALSE(input.isGamepadButtonDown(id, core::GamepadButton::south));
    EXPECT_FLOAT_EQ(input.getGamepadAxis(id, core::GamepadAxis::left_x), 0.0f);
    EXPECT_FALSE(input.rumbleGamepad(id, 1.0f, 1.0f, 10));

    EXPECT_TRUE(SDL_DetachVirtualJoystick(id));
    SDL_FlushEvents(SDL_EVENT_JOYSTICK_ADDED, SDL_EVENT_GAMEPAD_UPDATE_COMPLETE);
}
