#pragma once

//
// ScrollPipeline.h
// Orchestrates the full scroll processing pipeline:
//   MouseHook -> ScrollAnalyzer -> AccelerationCurve -> DragAnimator
//   -> SubPixelator -> ContactMapper -> DriverClient
//

#include "MouseHook.h"
#include "ScrollAnalyzer.h"
#include "AccelerationCurve.h"
#include "DragAnimator.h"
#include "SubPixelator.h"
#include "ContactMapper.h"
#include "DriverClient.h"
#include "TouchInjector.h"
#include "Settings.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <windows.h>

class ScrollPipeline {
public:
    ScrollPipeline(DriverClient& driver, const Settings& settings);
    ~ScrollPipeline();

    // Start intercepting scroll events
    bool start();

    // Stop intercepting scroll events
    void stop();

    // Update settings at runtime (thread-safe)
    void applySettings(const Settings& settings);

    bool isRunning() const { return m_running.load(); }

private:
    // Called by MouseHook on the hook thread — just enqueues
    void onScrollEvent(const ScrollEvent& ev);

    // Worker thread: dequeues events and runs the pipeline
    void scrollThreadFunc();

    // Process one scroll event on the scroll thread
    void processEvent(const ScrollEvent& ev);

    // Called from DragAnimator's animation thread each frame
    void onAnimationFrame(int dx, int dy, bool isLast);

    // Send a finger-lift report after idle timeout
    void scheduleLift();
    void cancelLift();

    // Components
    MouseHook        m_hook;
    ScrollAnalyzer   m_analyzer;
    AccelerationCurve m_accelCurve;
    DragAnimator     m_animator;
    SubPixelator     m_subPixelator;
    ContactMapper    m_contactMapper;
    DriverClient&    m_driver;
    TouchInjector    m_touchInjector;  // used in VirtualDriver mode as fallback

    // Settings (protected by m_settingsMutex)
    Settings         m_settings;
    std::mutex       m_settingsMutex;

    // Scroll event queue (hook thread -> scroll thread)
    std::queue<ScrollEvent>  m_eventQueue;
    std::mutex               m_queueMutex;
    std::condition_variable  m_queueCv;

    // Scroll worker thread
    std::thread      m_scrollThread;
    std::atomic<bool> m_running  { false };
    std::atomic<bool> m_shutdown { false };

    // Idle lift timer — uses a single persistent thread + condition variable
    std::thread              m_liftThread;
    std::mutex               m_liftMutex;
    std::condition_variable  m_liftCv;
    std::atomic<bool>        m_liftPending  { false };
    std::atomic<bool>        m_liftShutdown { false };
    DWORD                    m_liftDeadline { 0 };  // GetTickCount() deadline

    void liftThreadFunc();

    // Last scroll direction for direction-change detection
    int m_lastDirection = 0;
    long m_gestureY = 0;  // accumulated Y position for touch gesture

    // Idle timeout in milliseconds — after this, fingers are lifted
    static constexpr DWORD kIdleTimeoutMs = 80;
};
