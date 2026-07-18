import * as SliderPrimitive from "@radix-ui/react-slider";
import { cn } from "../../lib/utils";
export function Slider({ className, ...props }) {
  return (
    <SliderPrimitive.Root className={cn("ui-slider", className)} {...props}>
      <SliderPrimitive.Track className="slider-track">
        <SliderPrimitive.Range className="slider-range" />
      </SliderPrimitive.Track>
      <SliderPrimitive.Thumb className="slider-thumb" />
    </SliderPrimitive.Root>
  );
}
