import * as SwitchPrimitive from "@radix-ui/react-switch";
import { cn } from "../../lib/utils";
export function Switch({ className, ...props }) {
  return (
    <SwitchPrimitive.Root className={cn("ui-switch", className)} {...props}>
      <SwitchPrimitive.Thumb className="switch-thumb" />
    </SwitchPrimitive.Root>
  );
}
