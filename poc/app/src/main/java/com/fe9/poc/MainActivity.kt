package com.fe9.poc

import android.app.Activity
import android.os.Bundle
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
  }
  external fun nativeStart(assetManager: android.content.res.AssetManager)
  external fun nativeTouch(nx: Float, ny: Float, action: Int, pointerId: Int)
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
}
