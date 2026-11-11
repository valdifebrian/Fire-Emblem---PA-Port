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
