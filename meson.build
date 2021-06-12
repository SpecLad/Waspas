project('waspas', 'cpp', default_options : ['cpp_std=c++latest'])

compiler = meson.get_compiler('cpp')

manifest_inputs = []
link_args = []

if host_machine.system() == 'windows' and compiler.get_id() == 'msvc'
    utf8_manifest_path = meson.source_root() / 'utf-8.manifest'
    manifest_inputs += [utf8_manifest_path]
    link_args = [f'/MANIFESTINPUT:@utf8_manifest_path@']
endif

executable('waspas',
    'src/io.cxx',
    'src/io.ixx',
    'src/lexer.cxx',
    'src/lexer.ixx',
    'src/main.cxx',
    link_args : link_args,
    link_depends : manifest_inputs,
    extra_files : manifest_inputs,
)
