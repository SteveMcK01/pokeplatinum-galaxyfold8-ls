package org.pokeplatinum.port;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

/**
 * Puts the DS bottom screen and Gear companion on a second physical display when one is
 * connected, or attaches them to an inline bottom SurfaceView inside PlatinumActivity on
 * single-display foldable devices (such as the Galaxy Z Fold 8).
 */
final class SecondaryScreen implements DisplayManager.DisplayListener {
    private static final String TAG = "SecondaryScreen";

    interface InlineHost {
        void setInlineBottomScreenActive(boolean active);
    }

    private final Activity activity;
    private final DisplayManager displayManager;
    private final InlineHost inlineHost;
    private ScreenPresentation presentation;
    private boolean wanted;
    private boolean inlineActive;
    private boolean surfaceAttached;

    SecondaryScreen(Activity activity, InlineHost inlineHost) {
        this.activity = activity;
        this.inlineHost = inlineHost;
        this.displayManager = (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
        displayManager.registerDisplayListener(this, new Handler(Looper.getMainLooper()));
    }

    void show() {
        wanted = true;
        update();
    }

    void hide() {
        wanted = false;
        dismiss();
        setInlineActive(false);
    }

    void release() {
        hide();
        displayManager.unregisterDisplayListener(this);
    }

    /** Places the lower screen again after display or configuration changes. */
    void refresh() {
        if (wanted) {
            update();
        }
    }

    /** Binds a SurfaceView's lifecycle and touch events to libntr-display. */
    void bindSurfaceView(SurfaceView surfaceView) {
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                notifySurfaceChanged(holder.getSurface());
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                notifySurfaceDestroyed();
            }
        });
        surfaceView.setOnTouchListener(new SingleStylusTouchListener());
    }

    private void notifySurfaceChanged(Surface surface) {
        if (surface != null && surface.isValid()) {
            surfaceAttached = true;
            nativeSurfaceChanged(surface);
        }
    }

    private void notifySurfaceDestroyed() {
        if (surfaceAttached) {
            surfaceAttached = false;
            nativeSurfaceDestroyed();
        }
    }

    private void setInlineActive(boolean active) {
        if (inlineActive == active) {
            return;
        }
        inlineActive = active;
        if (!active) {
            notifySurfaceDestroyed();
        }
        if (inlineHost != null) {
            inlineHost.setInlineBottomScreenActive(active);
        }
    }

    private int ownDisplayId() {
        Display windowDisplay = activity.getWindow().getDecorView().getDisplay();
        if (windowDisplay != null) {
            return windowDisplay.getDisplayId();
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && activity.getDisplay() != null) {
            return activity.getDisplay().getDisplayId();
        }
        //noinspection deprecation
        return activity.getWindowManager().getDefaultDisplay().getDisplayId();
    }

    private Display pickDisplay() {
        int ownId = ownDisplayId();
        Display[] candidates = displayManager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
        if (candidates.length == 0) {
            candidates = displayManager.getDisplays();
        }
        for (Display display : candidates) {
            int id = display.getDisplayId();
            if (id == ownId || id == Display.DEFAULT_DISPLAY) {
                continue;
            }
            if (!display.isValid() || display.getState() == Display.STATE_OFF) {
                continue;
            }
            if ((display.getFlags() & Display.FLAG_PRIVATE) != 0) {
                continue;
            }
            return display;
        }
        return null;
    }

    private void update() {
        if (!wanted || activity.isFinishing() || activity.isDestroyed()) {
            dismiss();
            setInlineActive(false);
            return;
        }

        Display target = pickDisplay();
        if (target == null) {
            // Single physical display (e.g. Galaxy Z Fold 8): use inline split SurfaceView
            dismiss();
            Log.i(TAG, "Using inline bottom SurfaceView on display " + ownDisplayId());
            setInlineActive(true);
            return;
        }

        // External/secondary display detected: yield inline SurfaceView before opening Presentation
        setInlineActive(false);

        if (presentation != null && presentation.getDisplay().getDisplayId() == target.getDisplayId()) {
            return;
        }

        dismiss();
        try {
            presentation = new ScreenPresentation(activity, target, this);
            presentation.show();
            Log.i(TAG, "Bottom screen on display " + target.getDisplayId() + " (" + target.getName() + ")");
        } catch (WindowManager.InvalidDisplayException | WindowManager.BadTokenException e) {
            Log.w(TAG, "Could not show the bottom screen", e);
            presentation = null;
        }
    }

    private void dismiss() {
        if (presentation != null) {
            try {
                presentation.dismiss();
            } catch (RuntimeException ignored) {
            }
            presentation = null;
        }
        notifySurfaceDestroyed();
    }

    @Override
    public void onDisplayAdded(int displayId) {
        update();
    }

    @Override
    public void onDisplayRemoved(int displayId) {
        update();
    }

    @Override
    public void onDisplayChanged(int displayId) {
        update();
    }

    // Implemented in libntr (sim_display.cpp)
    static native void nativeSurfaceChanged(Surface surface);
    static native void nativeSurfaceDestroyed();
    static native void nativeTouch(boolean down, float x, float y);

    // The DS has one stylus: follow the first finger down until it lifts
    private static final class SingleStylusTouchListener implements View.OnTouchListener {
        private int touchPointerId = -1;

        @Override
        public boolean onTouch(View view, MotionEvent event) {
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_DOWN) {
                view.requestUnbufferedDispatch(event);
            }

            int index = event.getActionIndex();
            switch (action) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                    if (touchPointerId == -1) {
                        touchPointerId = event.getPointerId(index);
                        nativeTouch(true, event.getX(index), event.getY(index));
                    }
                    break;
                case MotionEvent.ACTION_MOVE: {
                    int pointerIndex = event.findPointerIndex(touchPointerId);
                    if (pointerIndex != -1) {
                        nativeTouch(true, event.getX(pointerIndex), event.getY(pointerIndex));
                    }
                    break;
                }
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP:
                    if (event.getPointerId(index) == touchPointerId) {
                        nativeTouch(false, event.getX(index), event.getY(index));
                        touchPointerId = -1;
                    }
                    break;
                case MotionEvent.ACTION_CANCEL:
                    nativeTouch(false, 0, 0);
                    touchPointerId = -1;
                    break;
                default:
                    break;
            }
            return true;
        }
    }

    private static final class ScreenPresentation extends Presentation {
        private final SecondaryScreen owner;

        ScreenPresentation(Context context, Display display, SecondaryScreen owner) {
            super(context, display);
            this.owner = owner;
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            Window window = getWindow();
            if (window != null) {
                window.addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                        | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }

            SurfaceView surfaceView = new SurfaceView(getContext());
            owner.bindSurfaceView(surfaceView);
            setContentView(surfaceView);
            hideSystemBars();
        }

        private void hideSystemBars() {
            Window window = getWindow();
            if (window == null) {
                return;
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                WindowInsetsController controller = window.getInsetsController();
                if (controller != null) {
                    controller.hide(WindowInsets.Type.systemBars());
                    controller.setSystemBarsBehavior(
                            WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                }
            } else {
                //noinspection deprecation
                window.getDecorView().setSystemUiVisibility(
                        View.SYSTEM_UI_FLAG_FULLSCREEN
                                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            }
        }
    }
}