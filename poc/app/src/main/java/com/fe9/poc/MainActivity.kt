package com.fe9.poc

import android.annotation.SuppressLint
import android.app.NativeActivity
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.widget.FrameLayout
import android.widget.TextView
import android.util.TypedValue

class MainActivity : NativeActivity(), SurfaceHolder.Callback {
  // libfe9poc.so auto-loads via android.app.lib_name; ANativeActivity_onCreate is the entry.
  external fun nativeTap(code: Int, tsNs: Long)
  external fun nativeSetSurface(surface: android.view.Surface?)

  @SuppressLint("ClickableViewAccessibility")
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    val root = FrameLayout(this)
    val gl = SurfaceView(this)
    gl.holder.addCallback(this)
    root.addView(gl, FrameLayout.LayoutParams(-1, -1))

    // touch overlay: dpad bottom-left, A/B bottom-right — big 96dp targets
    val dp = resources.displayMetrics.density
    fun size() = (96 * dp).toInt()
    fun edge() = (24 * dp).toInt()
    fun btn(label: String, code: Int): TextView = TextView(this).apply {
      text = label
      setTextColor(Color.WHITE)
      setBackgroundColor(0x66224466)
      gravity = Gravity.CENTER
      setTextSize(TypedValue.COMPLEX_UNIT_SP, 22f)
      setOnTouchListener { _, e ->
        if (e.actionMasked == MotionEvent.ACTION_DOWN) nativeTap(code, System.nanoTime())
        true
      }
    }
    fun add(v: View, x: Float, y: Float, alignBottom: Boolean = true) {
      val lp = FrameLayout.LayoutParams(size(), size())
      lp.gravity = if (alignBottom) Gravity.BOTTOM or Gravity.LEFT else Gravity.BOTTOM or Gravity.RIGHT
      lp.leftMargin = (x * dp).toInt(); lp.bottomMargin = (y * dp).toInt()
      root.addView(v, lp)
    }
    add(btn("◀", 1), 12f, 24f)
    add(btn("▶", 2), 120f, 24f)
    add(btn("▲", 3), 66f, 120f)
    add(btn("▼", 4), 66f, 24f)
    // right side uses right-gravity via separate helper
    fun addR(v: View, x: Float, y: Float) {
      val lp = FrameLayout.LayoutParams(size(), size())
      lp.gravity = Gravity.BOTTOM or Gravity.RIGHT
      lp.rightMargin = (x * dp).toInt(); lp.bottomMargin = (y * dp).toInt()
      root.addView(v, lp)
    }
    addR(btn("A", 5), 12f, 24f)   // A: toggle BGM mix B
    addR(btn("B", 6), 120f, 24f)  // B: restart anim
    setContentView(root)
  }

  override fun surfaceCreated(h: SurfaceHolder) { nativeSetSurface(h.surface) }
  override fun surfaceChanged(h: SurfaceHolder, f: Int, w: Int, ht: Int) {}
  override fun surfaceDestroyed(h: SurfaceHolder) { nativeSetSurface(null) }
}
