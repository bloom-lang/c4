require 'rubygems'
require 'C4'
require 'test/unit'

class TestTuple < Test::Unit::TestCase
  INPUT_DIR="input"
  OUTPUT_DIR="output"
  EXPECTED_DIR="expected"

  def setup
    @c4 = C4.new
  end

  def teardown
    @c4.destroy
  end

  def test_loop
    tests = Dir.entries("input").reject { |i| i.match(/^\./) }
    tests.each do |test|
      print "Running test \"#{test}\"..."
      input = ""
      output = ""
      File.open("#{INPUT_DIR}/#{test}").each_line do |line|
        if line =~ /^\\dump (.+)/
          @c4.install_str(input) unless input == ""
          output << @c4.dump_table($1).split("\n").sort.join("\n")
          input = ""
        else
          input << line
        end
      end
      puts " done"

      Dir.mkdir(OUTPUT_DIR) unless File.directory?(OUTPUT_DIR)
      File.open("#{OUTPUT_DIR}/#{test}", 'w') do |f|
        f.write(output)
      end
    end

    `diff -ur #{EXPECTED_DIR} #{OUTPUT_DIR} > regress.diffs`
    if $? == 0
      puts "All tests passed!"
    else
      puts "Tests failed; see regress.diffs"
    end
  end
end
