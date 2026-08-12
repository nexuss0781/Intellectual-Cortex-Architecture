# Stage 13 Preparation Research Sources

## NIST AI Risk Management Framework

NIST describes the AI RMF as a voluntary framework intended to improve the ability to incorporate trustworthiness considerations into the design, development, use, and evaluation of AI products, services, and systems. The Stage 13 protocol therefore separates governance, measurement, and management evidence and requires explicit claims boundaries rather than treating a passing harness as universal safety evidence.

Source: [NIST AI Risk Management Framework][1]

## NIST Generative AI Profile

The Stage 13 specification identifies the NIST Generative AI Profile as a companion profile for identifying generative-AI-specific risks. This preparation package uses that boundary as a rationale for separately recording safety, privacy, agency, misinformation, red-team, and incident evidence rather than aggregating them into one quality score.

Source: [NIST Generative AI Profile][2]

## OWASP Prompt Injection Guidance

OWASP describes direct and indirect prompt injection as inputs that can alter model behavior and potentially enable sensitive-data disclosure, unauthorized access, or unintended actions. Its mitigation guidance includes constrained behavior, validated output formats, input/output filtering, least privilege, human approval for high-risk actions, segregation of external content, and adversarial testing. Those controls are represented in the Stage 13 scope, reviewer, red-team, and consequential-action boundaries.

Source: [OWASP LLM01:2025 Prompt Injection][3]

## Evidence limitation

These sources inform the protocol design. They do not constitute an independent assessment of Nexuss. The Stage 13 preparation harness is an offline control-contract test; it does not create human pilot evidence, external review evidence, or public-service certification.

[1]: https://www.nist.gov/itl/ai-risk-management-framework "NIST AI Risk Management Framework"
[2]: https://doi.org/10.6028/NIST.AI.600-1 "NIST Generative AI Profile"
[3]: https://genai.owasp.org/llmrisk/llm01-prompt-injection/ "OWASP LLM01:2025 Prompt Injection"
