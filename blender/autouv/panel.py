import bpy


class AutoUV_PT_Panel(bpy.types.Panel):
    bl_label = "Auto UV Unwrap"
    bl_idname = "AUTOUV_PT_main"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "AutoUV"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        # --- Settings ---
        box = layout.box()
        box.label(text="Parameters", icon='PREFERENCES')
        box.prop(scene, "autouv_angle_threshold", text="Angle")
        box.prop(scene, "autouv_min_island", text="Min Island")
        box.prop(scene, "autouv_margin", text="Margin")

        # --- Seam Tools ---
        row = layout.row(align=True)
        row.label(text="Seams:")
        row.operator("autouv.mark_seam", text="Mark")
        row.operator("autouv.clear_seam", text="Clear")

        layout.separator()

        # --- Actions ---
        col = layout.column(align=True)
        col.scale_y = 1.5
        col.operator("autouv.unwrap", text="Unwrap Active", icon='UV')
        col.operator("autouv.batch_unwrap", text="Batch Unwrap", icon='OBJECT_DATAMODE')
        
        layout.separator()
        
        # --- Live Preview ---
        layout.operator("autouv.live_preview", text="Start Live Preview", icon='PLAY')

        layout.separator()
        
        # --- Metrics ---
        box = layout.box()
        box.label(text="Quality Metrics", icon='GRAPH') 
        col = box.column(align=True)
        col.label(text=f"Stretch: {scene.get('autouv_score_stretch', 0.0):.4f}")
        col.label(text=f"Coverage: {scene.get('autouv_score_coverage', 0.0):.2%}")
        col.label(text=f"Distortion: {scene.get('autouv_score_angle', 0.0):.2f}°")

def register():
    # Register the custom properties so they exist at runtime
    bpy.types.Scene.autouv_angle_threshold = bpy.props.FloatProperty(name="Angle", default=30.0, min=1.0)
    bpy.types.Scene.autouv_min_island = bpy.props.IntProperty(name="Min Island", default=10, min=1)
    bpy.types.Scene.autouv_margin = bpy.props.FloatProperty(name="Margin", default=0.02, min=0.0, max=0.5)
    
    bpy.utils.register_class(AutoUV_PT_Panel)

def unregister():
    bpy.utils.unregister_class(AutoUV_PT_Panel)
    del bpy.types.Scene.autouv_angle_threshold
    del bpy.types.Scene.autouv_min_island
    del bpy.types.Scene.autouv_margin