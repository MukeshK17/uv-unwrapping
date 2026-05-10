import bmesh
import bpy


class AutoUV_OT_MarkSeam(bpy.types.Operator):
    """Mark selected edges as seams"""
    bl_idname = "autouv.mark_seam"
    bl_label = "Mark Seam"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        # Only enable if in Edit Mode with a mesh selected
        return context.active_object and context.active_object.type == 'MESH' and context.active_object.mode == 'EDIT'

    def execute(self, context):
        obj = context.active_object
        # Get the internal mesh data directly
        bm = bmesh.from_edit_mesh(obj.data)
        
        count = 0
        for edge in bm.edges:
            if edge.select:
                edge.seam = True
                count += 1
        
        # Push changes back to Blender
        bmesh.update_edit_mesh(obj.data)
        
        if count > 0:
            self.report({'INFO'}, f"Marked {count} seams")
        else:
            self.report({'WARNING'}, "No edges selected")
        return {'FINISHED'}

class AutoUV_OT_ClearSeam(bpy.types.Operator):
    """Clear seams from selected edges"""
    bl_idname = "autouv.clear_seam"
    bl_label = "Clear Seam"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.type == 'MESH' and context.active_object.mode == 'EDIT'

    def execute(self, context):
        obj = context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        
        count = 0
        for edge in bm.edges:
            if edge.select:
                edge.seam = False
                count += 1
                
        bmesh.update_edit_mesh(obj.data)
        self.report({'INFO'}, f"Cleared {count} seams")
        return {'FINISHED'}

def register():
    bpy.utils.register_class(AutoUV_OT_MarkSeam)
    bpy.utils.register_class(AutoUV_OT_ClearSeam)

def unregister():
    bpy.utils.unregister_class(AutoUV_OT_MarkSeam)
    bpy.utils.unregister_class(AutoUV_OT_ClearSeam)