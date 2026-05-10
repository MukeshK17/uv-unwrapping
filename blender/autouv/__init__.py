bl_info = {
    "name": "Auto UV Unwrap",
    "author": "Your Name",
    "version": (1, 0),
    "blender": (3, 0, 0),
    "location": "View3D > Sidebar > AutoUV",
    "description": "Automatic UV unwrapping with LSCM, caching, and live preview",
    "category": "UV",
}

import bpy
import sys
import os
import importlib

# Add current directory to path to find 'uvwrap'
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.append(current_dir)


from . import cache
from . import seam_tools
from . import live_preview
from . import operator
from . import panel

if "operator" in locals():
    importlib.reload(cache)
    importlib.reload(seam_tools)
    importlib.reload(live_preview)
    importlib.reload(operator)
    importlib.reload(panel)

def register():
    cache.register()
    operator.register()
    seam_tools.register()
    live_preview.register()
    panel.register()

def unregister():
    panel.unregister()
    live_preview.unregister()
    seam_tools.unregister()
    operator.unregister()
    cache.unregister()

if __name__ == "__main__":
    register()