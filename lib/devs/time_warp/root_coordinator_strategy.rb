module DEVS
  module TimeWarp
    module RootCoordinatorStrategy
      def after_initialize
        @input_events = []
        @output_events = []

        # the sent, not yet received output events
        @pending_output_events = []

        @time_last = 0
      end

      def run
        child.dispatch(Event.new(:init, @time))
        @time_last = child.time_last

        loop do
          info "* Tick at: #{@time}, #{Time.now - @real_start_time} secs elapsed"
          child.dispatch(Event.new(:collect, @time))
          child.dispatch(Event.new(:star, @time))
          @time = child.time_next
          break if @time >= @duration
        end
      end
    end
  end
end
