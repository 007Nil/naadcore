# Agent Instructions

This document provides instructions for using the specialized agents in the NaadCore project.

## Available Agents

### 1. agents/bug-investigator
Use this agent when debugging issues with the NaadCore harmonium synthesizer. It's particularly useful for:
- MIDI event flow problems
- Audio driver issues  
- ALSA sequencer connectivity problems
- FluidSynth initialization failures

**Usage**: 
```
@agents/bug-investigator [problem description]
```

**Example**:
```
@agents/bug-investigator User reports no sound when playing Q49 keys. App shows MIDI events received but no audio output.
```

### 2. agents/architect-planner
Use this agent for high-level system design and implementation planning. It's ideal for:
- Designing new features
- Planning refactors
- Evaluating technology stacks
- Creating system architecture documents

**Usage**:
```
@agents/architect-planner [design requirement]
```

**Example**:
```
@agents/architect-planner Design a GUI for NaadCore with raga selection and bellows control
```

### 3. agents/codebase-analyst
Use this agent to understand the existing codebase structure and behavior. It's helpful for:
- Onboarding new developers
- Understanding module interactions
- Finding specific implementations
- Analyzing code patterns

**Usage**:
```
@agents/codebase-analyst [analysis request]
```

**Example**:
```
@agents/codebase-analyst Find all MIDI-related files and their responsibilities
```

### 4. agents/implementation-specialist
Use this agent for implementing new features or modifying existing code. It follows the project's established patterns and conventions. Ideal for:
- Adding new functionality
- Fixing bugs
- Refactoring code
- Writing tests

**Usage**:
```
@agents/implementation-specialist [implementation task]
```

**Example**:
```
@agents/implementation-specialist Implement raga selection feature in the harmonium plugin
```

### 5. agents/senior-code-reviewer
Use this agent for thorough code reviews. It ensures code quality, correctness, and adherence to best practices.

**Usage**:
```
@agents/senior-code-reviewer [code to review]
```

**Example**:
```
@agents/senior-code-reviewer Review the MIDI input handler implementation
```

## Best Practices

1. **Be Specific**: Include detailed problem descriptions for better agent responses
2. **Reference Context**: Mention relevant files, error messages, or recent changes
3. **Use Proper Formatting**: Use code blocks for file names and command examples
4. **Follow Up**: Respond to agent suggestions and ask clarifying questions
5. **Document Decisions**: Record important decisions made during agent interactions

## Agent Interaction Tips

- Agents can be called multiple times with different prompts
- The same agent can be used for multiple related tasks
- Agents work best with clear, focused requests
- Complex problems can be broken into smaller subtasks
- Results from agents should be integrated into the project documentation