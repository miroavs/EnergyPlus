from pyenergyplus.plugin import EnergyPlusPlugin


class ResizePSZToMatchProductAvailability(EnergyPlusPlugin):

    def select_discrete_nominal_airflow(self, state, initial_flow):
        for discrete_flow in [0.56628, 0.75504, 0.9438, 1.13256, 1.4157, 1.60446, 1.8879]:
            if initial_flow <= discrete_flow:
                return discrete_flow
        else:
            self.api.runtime.issue_warning(state, "Invalid airflow passed into Python Plugin")
            return 0.0

    def on_end_of_system_sizing(self, state):
        for zone_num in range(1, 11):
            var_handle = self.api.exchange.get_internal_variable_handle(
                state, "Intermediate Air System Main Supply Volume Flow Rate", "PSZ-AC_%s:%s" % (zone_num, zone_num)
            )
            actuator_handle = self.api.exchange.get_actuator_handle(
                state, "Sizing:System", "Main Supply Volume Flow Rate", "PSZ-AC_%s:%s" % (zone_num, zone_num)
            )
            initial_value = self.api.exchange.get_internal_variable_value(state, var_handle)
            new_value = self.select_discrete_nominal_airflow(state, initial_value)
            self.api.exchange.set_actuator_value(state, actuator_handle, new_value)
        return 0
