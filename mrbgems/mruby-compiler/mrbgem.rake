MRuby::Gem::Specification.new 'mruby-compiler' do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby developers'
  spec.summary = 'mruby compiler library'

  objs = %w[
    core/codegen
    core/codegen.yarp
    core/y.tab

    yarp/diagnostic
    yarp/missing
    yarp/node
    yarp/pack
    yarp/prettyprint
    yarp/regexp
    yarp/serialize
    yarp/token_type
    yarp/unescape
    yarp/yarp

    yarp/enc/ascii
    yarp/enc/big5
    yarp/enc/euc_jp
    yarp/enc/iso_8859_1
    yarp/enc/iso_8859_2
    yarp/enc/iso_8859_3
    yarp/enc/iso_8859_4
    yarp/enc/iso_8859_5
    yarp/enc/iso_8859_6
    yarp/enc/iso_8859_7
    yarp/enc/iso_8859_8
    yarp/enc/iso_8859_9
    yarp/enc/iso_8859_10
    yarp/enc/iso_8859_11
    yarp/enc/iso_8859_13
    yarp/enc/iso_8859_14
    yarp/enc/iso_8859_15
    yarp/enc/iso_8859_16
    yarp/enc/shared
    yarp/enc/shift_jis
    yarp/enc/unicode
    yarp/enc/windows_31j
    yarp/enc/windows_1251
    yarp/enc/windows_1252

    yarp/util/yp_buffer
    yarp/util/yp_char
    yarp/util/yp_conversion
    yarp/util/yp_list
    yarp/util/yp_state_stack
    yarp/util/yp_string_list
    yarp/util/yp_string
    yarp/util/yp_strpbrk
  ].map do |name|
    src = "#{dir}/#{name}.c"
    if build.cxx_exception_enabled?
      build.compile_as_cxx(src)
    else
      objfile("#{build_dir}/#{name}")
    end
  end
  build.libmruby_core_objs << objs
end

if MRuby::Build.current.name == "host"
  dir = __dir__
  lex_def = "#{dir}/core/lex.def"

  # Parser
  file "#{dir}/core/y.tab.c" => ["#{dir}/core/parse.y", lex_def] do |t|
    MRuby.targets["host"].yacc.run t.name, t.prerequisites.first
    replace_line_directive(t.name)
  end

  # Lexical analyzer
  file lex_def => "#{dir}/core/keywords" do |t|
    MRuby.targets["host"].gperf.run t.name, t.prerequisites.first
    replace_line_directive(t.name)
  end

  def replace_line_directive(path)
    content = File.read(path).gsub(%r{
      ^\#line\s+\d+\s+"\K.*(?="$) |             # #line directive
      ^/\*\s+Command-line:.*\s\K\S+(?=\s+\*/$)  # header comment in lex.def
    }x, &:relative_path)
    File.write(path, content)
  end
end
