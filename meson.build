project('waspas', 'cpp', default_options : ['cpp_std=c++latest'])

compiler = meson.get_compiler('cpp')

manifest_inputs = []
link_args = []

if host_machine.system() == 'windows' and compiler.get_id() == 'msvc'
    utf8_manifest_path = meson.source_root() / 'utf-8.manifest'
    manifest_inputs += [utf8_manifest_path]
    link_args = [f'/MANIFESTINPUT:@utf8_manifest_path@']
endif

waspas_exe = executable('waspas',
    'src/io.cxx',
    'src/io.ixx',
    'src/lexing.cxx',
    'src/lexing.ixx',
    'src/main.cxx',
    'src/parsing.cxx',
    'src/parsing.ixx',
    'src/reporting.ixx',
    link_args : link_args,
    link_depends : manifest_inputs,
    extra_files : manifest_inputs,
)

python = import('python')

python_installation = python.find_installation('python3')

test_env = environment()
test_env.set('WASPAS_TEST_EXE_PATH', waspas_exe.full_path())

test('Python tests', python_installation,
    args : ['--', files('test/run_tests.py')], env : test_env)
