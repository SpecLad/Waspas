project('waspas', 'cpp', default_options : ['cpp_std=c++latest'])

python = import('python')

python_installation = python.find_installation('python3')

compiler = meson.get_compiler('cpp')

manifest_inputs = []
link_args = []

if host_machine.system() == 'windows' and compiler.get_id() == 'msvc'
    utf8_manifest_path = meson.source_root() / 'utf-8.manifest'
    manifest_inputs += [utf8_manifest_path]
    link_args = [f'/MANIFESTINPUT:@utf8_manifest_path@']
endif

generate_nodes = custom_target(
    'generate_nodes',
    output : ['parsing-nodes.ixx'],
    command : [python_installation, '--', files('generate-nodes.py')],
    capture : true,
)

waspas_exe = executable('waspas',
    'src/io.cxx',
    'src/io.ixx',
    'src/lexing.cxx',
    'src/lexing.ixx',
    'src/main.cxx',
    'src/parsing.cxx',
    'src/parsing.ixx',
    'src/parsing-core.ixx',
    'src/reporting.ixx',
    'src/semantics.cxx',
    'src/semantics-analyze.cxx',
    'src/semantics.ixx',
    'src/utilities.ixx',
    generate_nodes,
    link_args : link_args,
    link_depends : manifest_inputs,
    extra_files : manifest_inputs,
)

test_env = environment()
test_env.set('WASPAS_TEST_EXE_PATH', waspas_exe.full_path())

test('Python tests', python_installation,
    args : ['--', files('test/run_tests.py')], env : test_env)
