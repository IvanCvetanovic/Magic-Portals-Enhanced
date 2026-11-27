"""game/main.cpp hands the engine the Vulkan answers of game/data/engine.json (planning step 281).

main.cpp cannot be linked into a suite, so its two lines are read as text: without the first, every run asks the engine's own Vulkan 1.2 and every older
device is turned away; without the second, a GPU that allows few storage buffers never gets the compact scene set.
"""

import os
import re
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


class TheWiring(unittest.TestCase):
    def setUp(self):
        with open(os.path.join(REPO, 'game', 'main.cpp'), encoding='utf-8') as f:
            self.lines = [line.strip() for line in f.read().split('\n')]

    def test_the_minimum_vulkan_version_reaches_the_manifest(self):
        # Zero is a value (Vulkan 1.0): the optional is tested, not its number.
        self.assertIn('if (tuning.minimumVulkanMinor) manifest.minimumVulkanMinor = *tuning.minimumVulkanMinor;', self.lines)

    def test_the_compact_scene_reaches_the_manifest(self):
        self.assertIn('manifest.compactSceneForOldGpus = tuning.compactSceneForOldGpus;', self.lines)


class TheShaders(unittest.TestCase):
    def test_every_shader_the_renderer_opens_is_shipped(self):
        renderer = os.path.join(REPO, 'engine', 'src', 'renderer')
        shaders = os.path.join(REPO, 'engine', 'assets', 'shaders')
        if not os.path.isdir(renderer) or not os.path.isdir(shaders):
            self.skipTest('the engine submodule is not checked out: the shader names were NOT checked')
        names = set()
        for name in os.listdir(renderer):
            if name.endswith('.cpp'):
                with open(os.path.join(renderer, name), encoding='utf-8') as f:
                    names.update(re.findall(r'"assets/shaders/([A-Za-z0-9_]+\.spv)"', f.read()))
        self.assertIn('frag_compact.spv', names)
        self.assertEqual(sorted(n for n in names if not os.path.isfile(os.path.join(shaders, n))), [])


if __name__ == '__main__':
    unittest.main()
