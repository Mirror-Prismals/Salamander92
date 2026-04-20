struct Uniforms {
    model: mat4x4<f32>,
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    mvp: mat4x4<f32>,
    color: vec4<f32>,
    topColor: vec4<f32>,
    bottomColor: vec4<f32>,
    params: vec4<f32>,
};

@group(0) @binding(0)
var<uniform> u: Uniforms;

struct FSIn {
    @location(0) uv: vec2<f32>,
};

@fragment
fn fs_main(input: FSIn) -> @location(0) vec4<f32> {
    let ndc = vec2<f32>(
        input.uv.x * 2.0 - 1.0,
        input.uv.y * 2.0 - 1.0
    );
    let projX = max(abs(u.projection[0].x), 0.0001);
    let projY = max(abs(u.projection[1].y), 0.0001);
    let viewRay = normalize(vec3<f32>(ndc.x / projX, ndc.y / projY, -1.0));
    let viewRot = mat3x3<f32>(u.view[0].xyz, u.view[1].xyz, u.view[2].xyz);
    let worldRay = normalize(transpose(viewRot) * viewRay);

    let horizonColor = clamp(mix(u.bottomColor.rgb, u.topColor.rgb, 0.5), vec3<f32>(0.0), vec3<f32>(1.0));
    let upMix = pow(clamp(max(worldRay.y, 0.0), 0.0, 1.0), 0.70);
    let downMix = pow(clamp(max(-worldRay.y, 0.0), 0.0, 1.0), 0.80);

    var color = horizonColor;
    if (worldRay.y >= 0.0) {
        color = mix(horizonColor, u.topColor.rgb, upMix);
    } else {
        color = mix(horizonColor, u.bottomColor.rgb, downMix);
    }
    return vec4<f32>(color, 1.0);
}
