<task>
  <simulate>
    <code>@ARGUMENT</code>
    <instructions>
      Simulate the execution of the provided code as realistically as possible. Take into account various inputs, including edge cases, and observe the outputs and any side effects. Consider the environment in which the code is intended to run, such as hardware constraints, network conditions, or user interactions, if applicable.
    </instructions>
  </simulate>
  <generate_document>
    <instructions>
      Based on the simulation, create a detailed document with the following sections:
    </instructions>
    <sections>
      <section>
        <title>System Behavior</title>
        <content>
          Provide a comprehensive description of how the system behaves. Include details on:
          - Functionality: What the code does and how it processes inputs to produce outputs.
          - Performance: Execution time, resource usage, and any bottlenecks observed.
          - Error Handling: How the code manages errors or unexpected inputs.
          - User Experience: If applicable, describe the user interface or interaction flow.
        </content>
      </section>
      <section>
        <title>Optimization Opportunities</title>
        <content>
          Identify areas where the code can be optimized. Consider:
          - Time Complexity: Are there inefficient algorithms that can be improved?
          - Space Complexity: Is there excessive memory usage that can be reduced?
          - Code Structure: Can the code be refactored for better readability and maintainability?
          - Resource Management: Are there ways to optimize CPU, memory, or I/O operations?
        </content>
      </section>
      <section>
        <title>Alternative Approaches</title>
        <content>
          Suggest different methods or strategies that could be employed to achieve the same or better results. This could include:
          - Alternative algorithms or data structures.
          - Different architectural patterns or design paradigms.
          - Leveraging external libraries or frameworks.
          - Parallelization or asynchronous processing, if applicable.
        </content>
      </section>
    </sections>
  </generate_document>
</task>