require 'rubygems'
require 'C4'
require 'test/unit'

class TestTuple < Test::Unit::TestCase
  class TestPair
    attr_reader :program, :commands, :expected
    def initialize(name)
      @commands = []
      @program = file_to_str("input/" + name)
      # sort the records on both sides.  this is ugly.
      @expected = file_to_str("expected/" + name).split("\n").sort.join("\n")
    end

    def file_to_str(file)
      file = File.new(file)
      str = ''
      while (line = file.gets)
        if (line =~ /^\\dump (.+)/)
          # side-effect onto array
          @commands.push($1)
        else
          str << line
        end
      end
      return str
    end
  end

  def setup
    @c4 = C4.new
  end

  def teardown
    @c4.destroy
  end

  def test_loop
    tests = Dir.entries("input").reject { |i| i.match(/^\./) }
    tests.each { |t| puts "test= #{t}" }
    tests.each do |test|
      tp = TestPair.new(test)
      @c4.install_str(tp.program)

      output = ''
      tp.commands.each do |c|
        output << @c4.dump_table(c)
      end

      assert_equal(tp.expected, output.split("\n").sort.join("\n"))
    end
  end
end
