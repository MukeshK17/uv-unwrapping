import time

import bpy


class AutoUV_OT_LivePreview(bpy.types.Operator):
    """Live Preview Mode: Auto-unwrap when mesh changes"""
    bl_idname = "autouv.live_preview"
    bl_label = "Start Live Preview"
    bl_options = {'REGISTER'}

    _timer = None
    _last_update = 0
    DEBOUNCE_TIME = 0.2  # 200ms debounce

    def modal(self, context, event):
        if event.type == 'ESC':
            return self.cancel(context)

        if event.type == 'TIMER':
            # In a real tool, verify depsgraph updates.
            # Continuously trigger every 200ms if active object is valid
            now = time.time()
            if now - self._last_update > self.DEBOUNCE_TIME:
                if context.active_object and context.active_object.type == 'MESH':
                    try:
                        # Call unwrap (caching handles efficiency)
                        bpy.ops.autouv.unwrap()
                        self._last_update = now
                    except: 
                        pass
        
        return {'PASS_THROUGH'}

    def execute(self, context):
        self.report({'INFO'}, "Live Preview Started (ESC to stop)")
        # Fire timer every 0.1s
        self._timer = context.window_manager.event_timer_add(0.1, window=context.window)
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def cancel(self, context):
        context.window_manager.event_timer_remove(self._timer)
        self.report({'INFO'}, "Live Preview Stopped")
        return {'FINISHED'}

def register():
    bpy.utils.register_class(AutoUV_OT_LivePreview)

def unregister():
    bpy.utils.unregister_class(AutoUV_OT_LivePreview)