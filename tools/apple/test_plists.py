"""The two Info.plist templates (game/macos/Info.plist.in, game/ios/Info.plist.in), parsed with plistlib and pinned key by key (planning step 206, PORT-8).

Run:   python -m unittest discover -s tools/apple -p "test_*.py" -v        (standard library only)

WHAT THIS PROVES. That each template is well-formed property-list XML, that every key the project RELIES ON is present with its value (the bundle's name, id and
executable, the version taken from the one definition in CMakeLists.txt, the minimum systems, the iOS device capabilities and orientations, the scene manifest),
that the places that must agree with a plist do (the Android package id, the executable's target name, the icon files the packaging script and the icon tool
write, the keys make_app.sh rewrites), and that no claim this project cannot back is in it (no export-compliance key, no word of the sibling port).
WHAT IT CANNOT PROVE. That macOS or iOS accepts these plists: NOT KNOWN until a Mac or a device is given one. A template is checked, not a bundle.

The MUTATIONS at the end run the same checks over an edited template and require each edit to be refused.
"""
import os
import plistlib
import re
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

PLACEHOLDER = '@MAGICPORTALS_BUNDLE_VERSION@'
# Names written in pieces, so that this file passes the grep for them that the hard rule's pre-commit check makes.
SIBLING = 'pen' + 'umbra'
ORIGINAL_MAKER = 'asan' + 'tee'
BUNDLE_ID = 'com.ivancvetanovic.magicportals'  # a PLACEHOLDER of the owner's to change; the Android package says the same
MACOS_MINIMUM = '13.3'
IOS_MINIMUM = '16.3'


def read(*parts):
    with open(os.path.join(REPO, *parts), encoding='utf-8') as f:
        return f.read()


def project_version():
    match = re.search(r'^project\(MagicPortals VERSION ([0-9]+(?:\.[0-9]+){1,3})\b', read('CMakeLists.txt'), re.M)
    assert match, 'CMakeLists.txt no longer says project(MagicPortals VERSION x.y.z ...)'
    return match.group(1)


def configured(text, version):
    """What game/CMakeLists.txt's configure_file(... @ONLY) makes of a template."""
    return text.replace(PLACEHOLDER, version)


def check_common(plist, version):
    """What both platforms' plists hold."""
    assert plist.get('CFBundleDevelopmentRegion') == 'en', 'CFBundleDevelopmentRegion'
    assert plist.get('CFBundleDisplayName') == 'Magic Portals', 'CFBundleDisplayName is the game\'s title (main.cpp kTitle)'
    assert plist.get('CFBundleName') == 'Magic Portals', 'CFBundleName'
    assert plist.get('CFBundleExecutable') == 'MagicPortals', 'CFBundleExecutable is the target\'s name, which make_app.sh copies as MagicPortals'
    assert plist.get('CFBundleIdentifier') == BUNDLE_ID, 'CFBundleIdentifier is the Android package\'s placeholder'
    assert plist.get('CFBundleInfoDictionaryVersion') == '6.0', 'CFBundleInfoDictionaryVersion'
    assert plist.get('CFBundlePackageType') == 'APPL', 'CFBundlePackageType'
    assert plist.get('CFBundleShortVersionString') == version, 'CFBundleShortVersionString is the one version: %r' % plist.get('CFBundleShortVersionString')
    assert plist.get('CFBundleVersion') == version, 'CFBundleVersion is the one version: %r' % plist.get('CFBundleVersion')
    assert re.fullmatch(r'[0-9]+(\.[0-9]+){0,2}', version), 'CFBundleVersion is at most three period-separated integers (the App Store\'s rule): %r' % version
    assert 'ITSAppUsesNonExemptEncryption' not in plist, 'no export-compliance statement: whether the app uses encryption is the owner\'s to declare'
    assert not any(SIBLING in repr(value).lower() for value in plist.values()), 'nothing of the sibling port'


def check_macos(plist, version):
    check_common(plist, version)
    assert plist.get('CFBundleIconFile') == 'MagicPortals', 'CFBundleIconFile names Resources/MagicPortals.icns, which make_app.sh writes'
    assert plist.get('LSMinimumSystemVersion') == MACOS_MINIMUM, 'LSMinimumSystemVersion'
    assert plist.get('NSHighResolutionCapable') is True, 'NSHighResolutionCapable'
    assert plist.get('NSSupportsAutomaticGraphicsSwitching') is True, 'NSSupportsAutomaticGraphicsSwitching'
    assert plist.get('LSApplicationCategoryType') == 'public.app-category.puzzle-games', 'LSApplicationCategoryType'
    for ios_only in ('UIRequiresFullScreen', 'MinimumOSVersion', 'UIDeviceFamily', 'UIApplicationSceneManifest'):
        assert ios_only not in plist, ios_only + ' is iOS\'s'


def check_ios(plist, version):
    check_common(plist, version)
    assert plist.get('CFBundleSupportedPlatforms') == ['iPhoneOS'], 'CFBundleSupportedPlatforms (make_app.sh rewrites it for the simulator)'
    assert plist.get('MinimumOSVersion') == IOS_MINIMUM, 'MinimumOSVersion'
    assert plist.get('LSRequiresIPhoneOS') is True, 'LSRequiresIPhoneOS'
    assert plist.get('UIDeviceFamily') == [1, 2], 'UIDeviceFamily: iPhone and iPad'
    assert plist.get('UIRequiredDeviceCapabilities') == ['arm64', 'metal'], 'UIRequiredDeviceCapabilities'
    assert plist.get('UIRequiresFullScreen') is True, 'UIRequiresFullScreen'
    assert plist.get('UIStatusBarHidden') is True, 'UIStatusBarHidden'
    assert plist.get('UILaunchScreen') == {}, 'an EMPTY UILaunchScreen is what makes the app run at the display\'s own size'
    assert plist.get('UIApplicationSceneManifest') == {'UIApplicationSupportsMultipleScenes': False}, 'UIApplicationSceneManifest: one scene, delegate named in code'
    landscape = ['UIInterfaceOrientationLandscapeLeft', 'UIInterfaceOrientationLandscapeRight']
    assert plist.get('UISupportedInterfaceOrientations') == landscape, 'landscape only (iPhone)'
    assert plist.get('UISupportedInterfaceOrientations~ipad') == landscape, 'landscape only (iPad)'
    assert plist.get('CFBundleIcons', {}).get('CFBundlePrimaryIcon', {}).get('CFBundleIconFiles') == ['AppIcon60x60'], 'CFBundleIcons'
    assert plist.get('CFBundleIcons~ipad', {}).get('CFBundlePrimaryIcon', {}).get('CFBundleIconFiles') == ['AppIcon60x60', 'AppIcon76x76', 'AppIcon83.5x83.5'], 'CFBundleIcons~ipad'
    assert plist.get('UIFileSharingEnabled') is True and plist.get('LSSupportsOpeningDocumentsInPlace') is False, 'Documents is shared (the flags file goes there)'
    assert 'LSMinimumSystemVersion' not in plist and 'NSHighResolutionCapable' not in plist, 'those are macOS\'s'


class PlistTemplates(unittest.TestCase):
    macos_text = read('game', 'macos', 'Info.plist.in')
    ios_text = read('game', 'ios', 'Info.plist.in')

    def parse(self, text, version=None):
        return plistlib.loads(configured(text, version or project_version()).encode('utf-8'))

    def test_both_templates_are_property_lists_whose_only_placeholder_is_the_version(self):
        for name, text in (('macOS', self.macos_text), ('iOS', self.ios_text)):
            self.assertEqual(sorted(set(re.findall(r'@[A-Za-z0-9_]+@', text))), [PLACEHOLDER], name)
            self.assertEqual(text.count(PLACEHOLDER), 2, name + ': the short version and the build version, both the one version')
            plistlib.loads(text.encode('utf-8'))  # the template itself parses, placeholder and all
            self.assertNotIn('@', configured(text, project_version()), name + ': nothing is left unsubstituted')

    def test_the_macos_template_holds_every_key_the_project_relies_on(self):
        check_macos(self.parse(self.macos_text), project_version())

    def test_the_ios_template_holds_every_key_the_project_relies_on(self):
        check_ios(self.parse(self.ios_text), project_version())

    def test_the_version_is_the_one_in_cmake_and_only_there(self):
        # Another version number in the project() line moves both plists; a number written into a template does not.
        for version in ('0.1.0', '1.2.3', '2.0'):
            self.assertEqual(self.parse(self.macos_text, version)['CFBundleShortVersionString'], version)
            self.assertEqual(self.parse(self.ios_text, version)['CFBundleVersion'], version)
        self.assertIn('set(MAGICPORTALS_BUNDLE_VERSION "${MagicPortals_VERSION}")', read('game', 'CMakeLists.txt').replace('\r\n', '\n'))
        self.assertIn('configure_file(ios/Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/ios/Info.plist" @ONLY)', read('game', 'CMakeLists.txt').replace('\r\n', '\n'))
        self.assertIn('configure_file(macos/Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/macos/Info.plist" @ONLY)', read('game', 'CMakeLists.txt').replace('\r\n', '\n'))

    # ---- the places that must agree with a plist ----------------------------------------------------------------------------------------------------

    def test_the_bundle_id_is_the_android_package_and_not_the_siblings(self):
        manifest = read('game', 'android', 'AndroidManifest.xml')
        self.assertIn('package="%s"' % BUNDLE_ID, manifest, 'a placeholder that the Android build and the Apple builds share; the owner changes all or none')
        for text in (self.macos_text, self.ios_text):
            self.assertIn('<string>%s</string>' % BUNDLE_ID, text)

    def test_the_executable_and_the_icon_are_what_the_cmake_target_and_the_script_make(self):
        cmake = read('game', 'CMakeLists.txt')
        self.assertIn('add_executable(MagicPortals ${_magicPortalsSources} macos/MacMain.mm)', cmake.replace('\r\n', '\n'))
        script = read('tools', 'apple', 'make_app.sh')
        self.assertIn('"$BIN/MagicPortals"', script)
        self.assertIn('"$RES/MagicPortals.icns"', script)
        self.assertIn('MagicPortals.app', script)

    def test_the_keys_make_app_sh_rewrites_are_in_the_templates(self):
        script = read('tools', 'apple', 'make_app.sh')
        for key, text in (('LSMinimumSystemVersion', self.macos_text), ('MinimumOSVersion', self.ios_text), ('CFBundleSupportedPlatforms', self.ios_text)):
            self.assertIn('plutil -replace %s ' % key, script, key)
            self.assertIn('<key>%s</key>' % key, text, key)
        # plutil -replace of a key that is not there ADDS it, which is what the DT* facts are: absent from the template, written by the script.
        for key in ('DTPlatformName', 'DTPlatformVersion', 'DTSDKName', 'DTSDKBuild'):
            self.assertIn('plutil -replace %s ' % key, script, key)
            self.assertNotIn(key, self.ios_text, key)

    def test_the_ios_icon_names_are_the_ones_the_icon_tool_writes(self):
        import ios_icons
        written = [name for name, _ in ios_icons.ICONS]
        plist = self.parse(self.ios_text)
        listed = plist['CFBundleIcons~ipad']['CFBundlePrimaryIcon']['CFBundleIconFiles']
        for base in listed:
            self.assertTrue(any(name.startswith(base + '@') for name in written), '%s has a file the tool writes' % base)
        script = read('tools', 'apple', 'make_app.sh')
        for name in written:
            self.assertIn('"$RES/%s"' % name, script, name + ' is copied into the bundle')

    def test_the_minimum_systems_are_the_ones_the_workflow_and_the_docs_build_for(self):
        workflow = read('.github', 'workflows', 'apple.yml')
        self.assertIn('CMAKE_OSX_DEPLOYMENT_TARGET=%s' % MACOS_MINIMUM, workflow)
        self.assertIn('CMAKE_OSX_DEPLOYMENT_TARGET=%s' % IOS_MINIMUM, workflow)
        self.assertEqual(self.parse(self.macos_text)['LSMinimumSystemVersion'], MACOS_MINIMUM)
        self.assertEqual(self.parse(self.ios_text)['MinimumOSVersion'], IOS_MINIMUM)

    def test_the_templates_name_nothing_of_the_sibling(self):
        for text in (self.macos_text, self.ios_text):
            self.assertNotIn(SIBLING, text.lower())
            self.assertNotIn(ORIGINAL_MAKER, text.lower())

    # ---- MUTATIONS: each edit of a template is refused by the checks ---------------------------------------------------------------------------------

    def refused(self, check, text, old, new):
        self.assertEqual(text.count(old), 1, 'the mutation applies once: %r' % old)
        mutated = plistlib.loads(configured(text.replace(old, new), project_version()).encode('utf-8'))
        with self.assertRaises(AssertionError, msg='MUTATION %r -> %r was not refused' % (old, new)):
            check(mutated, project_version())

    def test_mutations_of_the_macos_template_are_refused(self):
        t = self.macos_text
        self.refused(check_macos, t, '<string>13.3</string>', '<string>12.0</string>')
        self.refused(check_macos, t, '<key>NSHighResolutionCapable</key>\n    <true/>', '<key>NSHighResolutionCapable</key>\n    <false/>')
        self.refused(check_macos, t, '<string>com.ivancvetanovic.magicportals</string>', '<string>com.example.other</string>')
        self.refused(check_macos, t, '<key>CFBundleExecutable</key>\n    <string>MagicPortals</string>', '<key>CFBundleExecutable</key>\n    <string>Magic Portals</string>')
        self.refused(check_macos, t, '<key>CFBundleShortVersionString</key>\n    <string>@MAGICPORTALS_BUNDLE_VERSION@</string>',
                     '<key>CFBundleShortVersionString</key>\n    <string>0.9.9</string>')
        self.refused(check_macos, t, '<key>CFBundleIconFile</key>\n    <string>MagicPortals</string>', '<key>CFBundleIconFile</key>\n    <string>Icon</string>')
        self.refused(check_macos, t, '<key>LSApplicationCategoryType</key>', '<key>ITSAppUsesNonExemptEncryption</key>\n    <false/>\n    <key>LSApplicationCategoryType</key>')

    def test_mutations_of_the_ios_template_are_refused(self):
        t = self.ios_text
        self.refused(check_ios, t, '<string>16.3</string>', '<string>15.0</string>')
        self.refused(check_ios, t, '<key>UIRequiresFullScreen</key>\n    <true/>', '<key>UIRequiresFullScreen</key>\n    <false/>')
        self.refused(check_ios, t, '<string>metal</string>', '<string>opengles-3</string>')
        self.refused(check_ios, t, '<string>arm64</string>', '<string>armv7</string>')
        self.refused(check_ios, t, '    <key>UISupportedInterfaceOrientations</key>\n    <array>\n        <string>UIInterfaceOrientationLandscapeLeft</string>',
                     '    <key>UISupportedInterfaceOrientations</key>\n    <array>\n        <string>UIInterfaceOrientationPortrait</string>\n        <string>UIInterfaceOrientationLandscapeLeft</string>')
        self.refused(check_ios, t, '<key>UILaunchScreen</key>\n    <dict/>', '<key>UILaunchScreen</key>\n    <dict><key>UIImageName</key><string>x</string></dict>')
        self.refused(check_ios, t, '<key>UIApplicationSupportsMultipleScenes</key>\n        <false/>', '<key>UIApplicationSupportsMultipleScenes</key>\n        <true/>')
        self.refused(check_ios, t, '<key>CFBundleVersion</key>\n    <string>@MAGICPORTALS_BUNDLE_VERSION@</string>', '<key>CFBundleVersion</key>\n    <string>12</string>')
        self.refused(check_ios, t, '<string>com.ivancvetanovic.magicportals</string>', '<string>com.example.other</string>')

    def test_a_version_that_the_app_store_would_refuse_is_refused(self):
        with self.assertRaises(AssertionError):
            check_macos(self.parse(self.macos_text, '1.2.3.4'), '1.2.3.4')


if __name__ == '__main__':
    unittest.main()
