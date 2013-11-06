require 'devs'
require 'devs/parallel'
require 'devs/models'

DEVS.logger = Logger.new(STDOUT)
#DEVS.logger.level = Logger::INFO

DEVS.simulate do
  duration DEVS::INFINITY

  add_model DEVS::Models::Generators::SequenceGenerator, :name => :sequence

  add_model do
    name 'x^x'

    init do
      add_output_port :out_1
      add_input_port :in_1
    end

    when_input_received do |*messages|
      messages.each do |message|
        value = message.payload
        @result = value ** value
      end
      @sigma = 0
    end

    output do
      post @result, :out_1
    end

    after_output { @sigma = DEVS::INFINITY }
    time_advance { @sigma }
  end

  add_coupled_model do
    name :collector

    add_model DEVS::Models::Collectors::PlotCollector, :name => :plot
    add_model DEVS::Models::Collectors::CSVCollector, :name => :csv

    plug_input_port :a, :with_child => :csv, :and_child_port => 'x'
    plug_input_port :a, :with_child => :plot, :and_child_port => 'x'
    plug_input_port :b, :with_child => :csv, :and_child_port => 'x^x'
    plug_input_port :b, :with_child => :plot, :and_child_port => 'x^x'
  end

  plug :sequence, :with => 'x^x', :from => :value, :to => :in_1
  #plug :sequence, :with => :collector, :from => :value, :to => :a
  plug 'x^x', :with => :collector, :from => :out_1, :to => :a
end