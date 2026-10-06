import bpy, sys
def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    import importlib
    imp = importlib.import_module('io_scene_fbx.import_fbx')
    orig = imp.blen_read_light
    def safe(*a, **k):
        try: return orig(*a, **k)
        except AttributeError:
            return bpy.data.lights.new('fbxlight', 'POINT')
    imp.blen_read_light = safe
    bpy.ops.import_scene.fbx(filepath=path)
