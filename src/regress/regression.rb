require 'rubygems'
require 'fileutils'
require_relative './C4'

INPUT_DIR="input"
OUTPUT_DIR="output"
EXPECTED_DIR="expected"
DIFF_FILE="regress.diffs"

def make_output_dir
  FileUtils.remove_dir(OUTPUT_DIR) if File.directory?(OUTPUT_DIR)
  Dir.mkdir(OUTPUT_DIR)
end

def run_tests(c4, test_name)
  puts "===="
  tests = Dir.entries(INPUT_DIR).reject { |i| i.match(/^\./) }
  ran_tests = []
  tests.each do |test|
    next unless (test_name.nil? or test == test_name)
    ran_tests << test
    print "Running test \"#{test}\"..."
    input = ""
    output = ""
    File.open("#{INPUT_DIR}/#{test}").each_line do |line|
      if line =~ /^\\dump (.+)/
        c4.install_str(input) unless input == ""
        output << "**** \\dump \"#{$1}\" ****\n"
        output << c4.dump_table($1).split("\n").sort.join("\n")
        output << "\n"
        input = ""
      else
        input << line
      end
    end
    puts " done"

    File.open("#{OUTPUT_DIR}/#{test}", 'w') do |f|
      f.write(output)
    end
  end

  puts "===="
  num_fails = 0
  ran_tests.each do |test|
    `diff -ur #{EXPECTED_DIR}/#{test} #{OUTPUT_DIR}/#{test} >> #{DIFF_FILE}`
    if $? != 0
      num_fails += 1
      puts "Test \"#{test}\" failed!"
    end
  end

  if num_fails == 0
    puts "All #{ran_tests.length} tests passed"
  else
    puts "#{num_fails} TEST#{num_fails > 1 ? "S" : ""} FAILED; see regress.diffs"
  end
end

make_output_dir
File.delete(DIFF_FILE) if File.exists?(DIFF_FILE)
c = C4.new
run_tests(c, ARGV[0])
c.destroy
