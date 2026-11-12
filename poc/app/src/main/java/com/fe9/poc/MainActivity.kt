package com.fe9.poc

import android.app.Activity
import android.os.Bundle
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout

/* Plain Activity + SurfaceView: the view tree covers the whole window, so
 * OnTouchListener receives every pointer and nativeTouch gets coords in the
 * same normalized space the overlay buttons are drawn in (the C side
 * letterboxes its GL viewport inside this surface identically).
 * NOTE: do NOT switch back to NativeActivity — its AInputQueue path left the
 * window touchableRegion empty on this device (input silently dropped). */
class MainActivity : Activity(), SurfaceHolder.Callback {
  companion object {
    init {
      try { System.loadLibrary("fe9poc") }
      catch (e: Throwable) { android.util.Log.e("fe9poc", "loadLibrary failed", e) }
    }
    /* Mirror of PadButton/PadAxis in app/src/main/cpp/main.cpp — ABI order, NEVER reorder. */
    const val PADB_A = 0; const val PADB_B = 1; const val PADB_X = 2; const val PADB_Y = 3
    const val PADB_L1 = 4; const val PADB_R1 = 5; const val PADB_L2 = 6; const val PADB_R2 = 7
    const val PADB_SELECT = 8; const val PADB_START = 9; const val PADB_L3 = 10; const val PADB_R3 = 11
    const val PADB_GUIDE = 12; const val PADB_DPUP = 13; const val PADB_DPDOWN = 14
    const val PADB_DPLEFT = 15; const val PADB_DPRIGHT = 16
    const val PADAX_LX = 0; const val PADAX_LY = 1; const val PADAX_RX = 2; const val PADAX_RY = 3
    const val PADAX_LT = 4; const val PADAX_RT = 5; const val PADAX_HATX = 6; const val PADAX_HATY = 7
  }
  external fun nativeStart(assetManager: android.content.res.AssetManager)
  external fun nativeTouch(nx: Float, ny: Float, action: Int, pointerId: Int)
  external fun nativePadButton(button: Int, pressed: Boolean)
  external fun nativePadAxis(axis: Int, value: Float)
  external fun nativeSetSurface(surface: Surface?)

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    /* edge-to-edge: system bars hidden (immersive sticky), cutout shortEdges ->
       surface = full display for ANY modern ratio (20:9 phones, 16:10 tablets);
       engine reads surface size every frame so the GL viewport adapts live. */
    val p = window.attributes
    if (android.os.Build.VERSION.SDK_INT >= 28)
      p.layoutInDisplayCutoutMode = android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
    window.attributes = p
    @Suppress("DEPRECATION")
    window.decorView.systemUiVisibility = (
      android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
      or android.view.View.SYSTEM_UI_FLAG_FULLSCREEN
      or android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
      or android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
      or android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
      or android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE)
    nativeStart(assets)
    val root = FrameLayout(this)
    val gl = SurfaceView(this)
    gl.holder.addCallback(this)
    gl.setOnTouchListener { _, e ->
      val w = gl.width.toFloat(); val h = gl.height.toFloat()
      for (i in 0 until e.pointerCount) {
        val a = when (e.actionMasked) {
          MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> 0
          MotionEvent.ACTION_MOVE -> 1
          MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> 2
          else -> 3 // CANCEL
        }
        nativeTouch(e.getX(i) / w, e.getY(i) / h, a, e.getPointerId(i))
      }
      true
    }
    root.addView(gl, FrameLayout.LayoutParams(-1, -1))
    setContentView(root)
  }

  override fun surfaceCreated(h: SurfaceHolder) { nativeSetSurface(h.surface) }
  override fun surfaceChanged(h: SurfaceHolder, f: Int, w: Int, ht: Int) {}
  override fun surfaceDestroyed(h: SurfaceHolder) { nativeSetSurface(null) }

  /* ---------- physical gamepad (gamepad-only builds fall back to touch 100%:
   * none of these fire when no pad sends events; SurfaceView consumes nothing
   * so dispatchGenericMotionEvent on the Activity sees the pad first). ---- */

  /* actionButton int -> our PADB_* index; -1 = unmapped */
  private fun buttonToPad(btn: Int): Int = when (btn) {
    KeyEvent.KEYCODE_BUTTON_A -> PADB_A
    KeyEvent.KEYCODE_BUTTON_B -> PADB_B
    KeyEvent.KEYCODE_BUTTON_X -> PADB_X
    KeyEvent.KEYCODE_BUTTON_Y -> PADB_Y
    KeyEvent.KEYCODE_BUTTON_L1 -> PADB_L1
    KeyEvent.KEYCODE_BUTTON_R1 -> PADB_R1
    KeyEvent.KEYCODE_BUTTON_L2 -> PADB_L2
    KeyEvent.KEYCODE_BUTTON_R2 -> PADB_R2
    KeyEvent.KEYCODE_BUTTON_SELECT -> PADB_SELECT
    KeyEvent.KEYCODE_BUTTON_START -> PADB_START
    KeyEvent.KEYCODE_BUTTON_THUMBL -> PADB_L3
    KeyEvent.KEYCODE_BUTTON_THUMBR -> PADB_R3
    KeyEvent.KEYCODE_BUTTON_MODE -> PADB_GUIDE
    else -> -1
  }

  private fun sendPadAxes(ev: MotionEvent) {
    nativePadAxis(PADAX_LX, ev.getAxisValue(MotionEvent.AXIS_X))
    nativePadAxis(PADAX_LY, ev.getAxisValue(MotionEvent.AXIS_Y))
    nativePadAxis(PADAX_RX, ev.getAxisValue(MotionEvent.AXIS_Z))
    nativePadAxis(PADAX_RY, ev.getAxisValue(MotionEvent.AXIS_RZ))
    /* triggers: devices report rest at -1 (Xbox-style) or 0; map raw -1..1 -> 0..1 */
    nativePadAxis(PADAX_LT, ((ev.getAxisValue(MotionEvent.AXIS_LTRIGGER) + 1f) * 0.5f).coerceIn(0f, 1f))
    nativePadAxis(PADAX_RT, ((ev.getAxisValue(MotionEvent.AXIS_RTRIGGER) + 1f) * 0.5f).coerceIn(0f, 1f))
    nativePadAxis(PADAX_HATX, ev.getAxisValue(MotionEvent.AXIS_HAT_X))
    nativePadAxis(PADAX_HATY, ev.getAxisValue(MotionEvent.AXIS_HAT_Y))
  }

  override fun dispatchGenericMotionEvent(ev: MotionEvent): Boolean {
    val src = ev.source
    if (src and (InputDevice.SOURCE_GAMEPAD or InputDevice.SOURCE_JOYSTICK) == 0)
      return super.dispatchGenericMotionEvent(ev)
    when (ev.actionMasked) {
      MotionEvent.ACTION_BUTTON_PRESS, MotionEvent.ACTION_BUTTON_RELEASE -> {
        val b = buttonToPad(ev.actionButton)
        if (b >= 0) {
          nativePadButton(b, ev.actionMasked == MotionEvent.ACTION_BUTTON_PRESS)
          return true
        }
      }
      MotionEvent.ACTION_MOVE -> { sendPadAxes(ev); return true }
    }
    return super.dispatchGenericMotionEvent(ev)
  }

  /* safety net: also handle generic motion if it arrives via the focused-view
   * chain instead of the dispatch intercept */
  override fun onGenericMotionEvent(ev: MotionEvent): Boolean = dispatchGenericMotionEvent(ev)

  /* legacy path: dpad + old-style gamepad keys surface as key events
   * (also covers the only keyboard-adjacent route; KEY repeat spam is safe —
   * C++ ignores press-while-held for edges and repeats dpad on its own timer) */
  private fun keyToPad(code: Int): Int {
    // [P3-SCN-001] scene dev-triggers; when() kept for the rest
    if (code == KeyEvent.KEYCODE_T) return PADB_X      // push dialog scene
    if (code == KeyEvent.KEYCODE_M) return PADB_Y      // push battle scene (boss mix)
    return when (code) {
    KeyEvent.KEYCODE_DPAD_UP -> PADB_DPUP
    KeyEvent.KEYCODE_DPAD_DOWN -> PADB_DPDOWN
    KeyEvent.KEYCODE_DPAD_LEFT -> PADB_DPLEFT
    KeyEvent.KEYCODE_DPAD_RIGHT -> PADB_DPRIGHT
    KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_SPACE -> PADB_A
    KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_ESCAPE -> PADB_B
    KeyEvent.KEYCODE_BUTTON_X -> PADB_X
    KeyEvent.KEYCODE_BUTTON_Y -> PADB_Y
    KeyEvent.KEYCODE_BUTTON_L1 -> PADB_L1
    KeyEvent.KEYCODE_BUTTON_R1 -> PADB_R1
    KeyEvent.KEYCODE_BUTTON_L2 -> PADB_L2
    KeyEvent.KEYCODE_BUTTON_R2 -> PADB_R2
    KeyEvent.KEYCODE_BUTTON_SELECT -> PADB_SELECT
    KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_ENTER -> PADB_START
    KeyEvent.KEYCODE_BACK -> PADB_GUIDE /* scene pop / refuse-to-exit; consumed in C++ [P3-SCN-001] */
    KeyEvent.KEYCODE_BUTTON_THUMBL -> PADB_L3
    KeyEvent.KEYCODE_BUTTON_THUMBR -> PADB_R3
    else -> -1
  }
  }

  override fun onKeyDown(code: Int, ev: KeyEvent): Boolean {
    val b = keyToPad(code)
    android.util.Log.i("FE9PoC", "keyDown code=$code pad=$b")
    if (b >= 0) {
      if (ev.repeatCount == 0) nativePadButton(b, true)
      return true
    }
    return super.onKeyDown(code, ev)
  }

  override fun onKeyUp(code: Int, ev: KeyEvent): Boolean {
    val b = keyToPad(code)
    if (b >= 0) { nativePadButton(b, false); return true }
    return super.onKeyUp(code, ev)
  }
}
