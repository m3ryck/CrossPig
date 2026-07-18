import { cn } from "../../lib/utils";
export function Badge({ className, ...props }) {
  return <span className={cn("ui-badge", className)} {...props} />;
}
