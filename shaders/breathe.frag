#ifdef GL_ES
precision mediump float;
#endif

uniform sampler2D u_haloTex;
uniform float     u_time;
uniform float     u_cpuUsage;
uniform float     u_baseSpeed;
uniform float     u_scaleFactor;
uniform float     u_minAlpha;
uniform float     u_warmThreshold;
uniform vec3      u_warmColor;
uniform float     u_warmIntensity;
uniform float     u_forceSwapRB;

varying vec2 v_texCoord;

void main() {
    vec4 texel = texture2D(u_haloTex, v_texCoord);

    // Breathing wave: speed increases with CPU load
    float speed = u_baseSpeed + u_cpuUsage * u_scaleFactor;
    float wave  = 0.5 + 0.5 * sin(u_time * speed);

    // Alpha modulation: oscillates between minAlpha and 1.0
    float alpha = mix(u_minAlpha, 1.0, wave);

    // Warm tint overlay when CPU exceeds threshold
    vec3 color = texel.rgb;
    if (u_cpuUsage > u_warmThreshold) {
        float t = (u_cpuUsage - u_warmThreshold) / (1.0 - u_warmThreshold);
        color = mix(color, u_warmColor, t * u_warmIntensity);
    }

    // Safety net: swap R and B if force_swap_rb is enabled in character.json
    if (u_forceSwapRB > 0.5)
        color.rgb = color.bgr;

    gl_FragColor = vec4(color, texel.a * alpha);
}
