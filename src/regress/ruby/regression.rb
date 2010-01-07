require 'rubygems'
require 'C4'
require 'test/unit'

class TestTuple < Test::Unit::TestCase
  class TestPair
    attr_reader :program, :commands, :witness
    def initialize(name)
      @commands = Array.new
      @program = file_to_str("test/" + name)
      # sort the records on both sides.  this is ugly.
      @witness = file_to_str("witness/" + name).split("\n").sort.join("\n")
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

  def test_loop
    tests = Dir.entries("test").reject { |i| i.match(/^\./) }
    tests.each { |t| puts "test= #{t}" }
    tests.each do |test|
      tp = TestPair.new(test)
      @c4.install_str(tp.program)

      output = ''
      tp.commands.each do |c|
        output << @c4.dump_table(c)
      end

      assert_equal(tp.witness, output.split("\n").sort.join("\n"))
    end
  end
end
