module DEVS
  module Classic
    class Processor
      include Logging
      attr_accessor :parent
      attr_reader :model, :time_next, :time_last

      alias_method :tn, :time_next
      alias_method :tl, :time_last

      def initialize(model)
        @model = model
        @time_next = 0
        @time_last = 0
        @events_count = Hash.new(0)
      end

      def stats
        stats = @events_count.dup
        stats[:total] = stats.values.reduce(&:+)
        info "    #{self.model.name}: #{stats}"
        @events_count
      end

      def receive(event)
        @events_count[event.type] += 1
        info "#{self.model.name} (tn: #{@time_next}, tl: #{@time_last}) received\
  event at time #{event.time} of type #{event.type}"

        case event.type
        when :i then init_event(event)
        when :* then star_event(event)
        when :x then input_event(event)
        when :y then output_event(event)
        end
      end
      alias_method :dispatch, :receive
    end
  end
end
